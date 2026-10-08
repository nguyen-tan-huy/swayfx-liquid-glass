// Smooth and inertial scrolling, done in the compositor (swayctl patch).
//
// Touchpad (finger source): deltas are passed through a short low-pass so
// irregular polling doesn't show, and after the fingers lift the measured
// velocity keeps scrolling and decays with friction. The finger "stop" is
// held back until the coast ends, so clients that do their own kinetic
// scrolling see a stop with ~zero velocity and don't add a second coast.
//
// Wheel: each notch becomes a short glide (continuous source, no v120) whose
// total distance equals the notch, so speed is unchanged, only spread out.
// The first notches of a burst ramp up (ramp ms / power), like the python
// smoothscroll daemon this replaces.
//
// Output is emitted at the refresh rate of the output under the cursor.
// Windows marked `scroll_native enable` (e.g. by for_window) get the raw
// events. Off unless `input <id> smooth_scroll enabled`.
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include "log.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/input/smooth_scroll.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"

#define DEFAULT_FRICTION 0.98f   // per 1/240 s
#define DEFAULT_RAMP_MS 290.0f
#define DEFAULT_RAMP_POWER 0.8f
#define RAMP_FLOOR 0.45
#define BURST_RESET_MS 400
#define DEFAULT_SMOOTHING 0.4f   // share of the pending touchpad delta sent per 1/240 s
#define MIN_VELOCITY 4.0         // px/s below which a coast ends
#define RETOUCH_GRACE_MS 120     // a touch this soon after a lift continues the gesture
// A notch arrives as 15 (libinput "degrees") but apps scroll ~3 lines, about
// 60 px, for one wheel step; as pixels it must be scaled to keep the speed.
#define WHEEL_PIXELS_PER_DELTA 4.0
// Pointer travel (layout px) that ends a glide: clients latch a scroll
// sequence to the widget it started on until axis_stop, so the next notch
// must start a fresh sequence where the pointer now is.
#define MOTION_STOP_PX 6.0

struct axis_state {
	double pending;    // touchpad: received but not yet sent
	double velocity;   // px/s; coasting after release, or wheel glide
	double estimate;   // touchpad: smoothed input velocity while held
	bool held;         // fingers down on this axis
	bool coasting;
	uint32_t last_ms;  // last input on this axis
	uint32_t burst_start_ms;
	uint32_t lift_ms;
};

struct sway_smooth_scroll {
	struct sway_cursor *cursor;
	struct wl_event_source *timer;
	struct axis_state axis[2]; // WL_POINTER_AXIS_VERTICAL_SCROLL, HORIZONTAL
	enum wl_pointer_axis_source source;
	enum wl_pointer_axis_relative_direction direction[2];
	float friction, ramp_ms, ramp_power, smoothing;
	uint32_t last_tick_ms;
	double anchor_x, anchor_y; // cursor position at the last input
	bool running;
};

static uint32_t now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// A fixed 250 Hz, not the refresh rate: an untimed timer at the frame
// period drifts against vblank and every few frames two land in one (judder).
// Finer steps than any display shows keep each frame's share even.
#define TICK_MS 4

static int frame_interval_ms(struct sway_smooth_scroll *smooth) {
	(void)smooth;
	return TICK_MS;
}

static void send(struct sway_smooth_scroll *smooth, enum wl_pointer_axis orientation,
		double delta, enum wl_pointer_axis_source source) {
	wlr_seat_pointer_notify_axis(smooth->cursor->seat->wlr_seat, now_ms(), orientation,
		delta, 0, source, smooth->direction[orientation]);
}

// The source the client sees: wheel glides go out as continuous.
static enum wl_pointer_axis_source out_source(struct sway_smooth_scroll *smooth) {
	return smooth->source == WL_POINTER_AXIS_SOURCE_FINGER
		? WL_POINTER_AXIS_SOURCE_FINGER : WL_POINTER_AXIS_SOURCE_CONTINUOUS;
}

static void schedule(struct sway_smooth_scroll *smooth) {
	if (!smooth->running) {
		smooth->running = true;
		smooth->last_tick_ms = now_ms();
		wl_event_source_timer_update(smooth->timer, frame_interval_ms(smooth));
	}
}

static int handle_tick(void *data) {
	struct sway_smooth_scroll *smooth = data;
	uint32_t now = now_ms();
	double dt = (now - smooth->last_tick_ms) / 1000.0;
	if (dt <= 0) {
		dt = 0.001;
	}
	smooth->last_tick_ms = now;
	enum wl_pointer_axis_source source = out_source(smooth);
	// friction is defined per 1/240 s so values match the python daemon
	double decay = pow(smooth->friction, dt * 240.0);
	bool busy = false, sent = false;

	for (int o = 0; o < 2; o++) {
		struct axis_state *a = &smooth->axis[o];
		double out = 0;
		bool stop = false;
		if (a->held) {
			// smoothing is per 1/240 s like friction; scale it to this tick
			double share = 1.0 - pow(1.0 - smooth->smoothing, dt * 240.0);
			out = a->pending * share;
			a->pending -= out;
			busy = true;
		} else if (a->pending != 0 || a->coasting) {
			// whatever was still pending, then the coast
			out = a->pending + a->velocity * dt;
			a->pending = 0;
			a->velocity *= decay;
			if (fabs(a->velocity) < MIN_VELOCITY) {
				a->velocity = 0;
				// end the sequence for wheel glides too, or the client
				// keeps routing later scrolls to the widget it latched
				stop = a->coasting;
				a->coasting = false;
			} else {
				busy = true;
			}
		}
		if (out != 0) {
			send(smooth, o, out, source);
			sent = true;
		}
		if (stop) {
			send(smooth, o, 0, source); // axis_stop
			sent = true;
		}
	}
	if (sent) {
		// every group of axis events ends with a frame
		wlr_seat_pointer_notify_frame(smooth->cursor->seat->wlr_seat);
	}
	if (busy) {
		wl_event_source_timer_update(smooth->timer, frame_interval_ms(smooth));
	} else {
		smooth->running = false;
	}
	return 0;
}

struct sway_smooth_scroll *smooth_scroll_create(struct sway_cursor *cursor) {
	struct sway_smooth_scroll *smooth = calloc(1, sizeof(*smooth));
	if (!smooth) {
		return NULL;
	}
	smooth->cursor = cursor;
	smooth->timer = wl_event_loop_add_timer(server.wl_event_loop, handle_tick, smooth);
	smooth->friction = DEFAULT_FRICTION;
	smooth->ramp_ms = DEFAULT_RAMP_MS;
	smooth->ramp_power = DEFAULT_RAMP_POWER;
	smooth->smoothing = DEFAULT_SMOOTHING;
	return smooth;
}

void smooth_scroll_destroy(struct sway_smooth_scroll *smooth) {
	if (!smooth) {
		return;
	}
	wl_event_source_remove(smooth->timer);
	free(smooth);
}

void smooth_scroll_stop(struct sway_smooth_scroll *smooth) {
	if (!smooth) {
		return;
	}
	bool sent = false;
	for (int o = 0; o < 2; o++) {
		struct axis_state *a = &smooth->axis[o];
		bool was = a->coasting || a->pending != 0;
		a->pending = a->velocity = 0;
		a->coasting = false;
		if (was) {
			send(smooth, o, 0, out_source(smooth));
			sent = true;
		}
	}
	if (sent) {
		wlr_seat_pointer_notify_frame(smooth->cursor->seat->wlr_seat);
	}
}

void smooth_scroll_motion(struct sway_smooth_scroll *smooth) {
	if (!smooth || !smooth->running
			|| smooth->axis[0].held || smooth->axis[1].held) {
		return;
	}
	double dx = smooth->cursor->cursor->x - smooth->anchor_x;
	double dy = smooth->cursor->cursor->y - smooth->anchor_y;
	if (dx * dx + dy * dy > MOTION_STOP_PX * MOTION_STOP_PX) {
		smooth_scroll_stop(smooth);
	}
}

static bool native_under_cursor(struct sway_cursor *cursor) {
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(cursor->seat,
		cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	return node && node->type == N_CONTAINER && node->sway_container->scroll_native;
}

bool smooth_scroll_axis(struct sway_smooth_scroll *smooth, struct input_config *ic,
		struct wlr_pointer_axis_event *event, double delta, int32_t delta_discrete) {
	if (!smooth || !ic || ic->smooth_scroll != 1) {
		return false;
	}
	if (event->source != WL_POINTER_AXIS_SOURCE_FINGER
			&& event->source != WL_POINTER_AXIS_SOURCE_WHEEL) {
		return false; // continuous (trackpoint) and tilt: leave alone
	}
	if (native_under_cursor(smooth->cursor)) {
		smooth_scroll_stop(smooth);
		return false;
	}
	smooth->friction = ic->scroll_friction != FLT_MIN ? ic->scroll_friction : DEFAULT_FRICTION;
	smooth->ramp_ms = ic->scroll_ramp_ms != FLT_MIN ? ic->scroll_ramp_ms : DEFAULT_RAMP_MS;
	smooth->ramp_power = ic->scroll_ramp_power != FLT_MIN ? ic->scroll_ramp_power : DEFAULT_RAMP_POWER;
	smooth->smoothing = ic->scroll_smoothing != FLT_MIN ? ic->scroll_smoothing : DEFAULT_SMOOTHING;

	enum wl_pointer_axis o = event->orientation;
	struct axis_state *a = &smooth->axis[o];
	uint32_t t = event->time_msec;
	if (smooth->source != event->source) {
		smooth_scroll_stop(smooth); // switched between touchpad and wheel
	}
	smooth->source = event->source;
	smooth->direction[o] = event->relative_direction;
	smooth->anchor_x = smooth->cursor->cursor->x;
	smooth->anchor_y = smooth->cursor->cursor->y;

	if (event->source == WL_POINTER_AXIS_SOURCE_FINGER) {
		if (delta == 0) { // fingers lifted on this axis
			if (a->held) {
				a->held = false;
				a->lift_ms = t;
				// a pause before lifting means the user stopped: no coast
				a->velocity = a->last_ms && t - a->last_ms < 60 ? a->estimate : 0;
				a->coasting = true;
				schedule(smooth);
			}
			return true;
		}
		if (!a->held) {
			bool retouch = a->lift_ms && t - a->lift_ms < RETOUCH_GRACE_MS;
			a->held = true;
			if (!retouch) {
				a->estimate = 0;
				a->velocity = 0;
				a->coasting = false;
				a->last_ms = 0;
			} else {
				a->coasting = false;
			}
		}
		double dt = a->last_ms ? (t - a->last_ms) / 1000.0 : 0;
		if (dt > 0 && dt < 0.1) {
			a->estimate += (delta / dt - a->estimate) * 0.4;
		}
		a->last_ms = t;
		a->pending += delta;
		schedule(smooth);
		return true;
	}

	// wheel notch
	if (!a->last_ms || t - a->last_ms > BURST_RESET_MS) {
		a->burst_start_ms = t;
	}
	a->last_ms = t;
	double progress = smooth->ramp_ms > 0 ? (t - a->burst_start_ms) / smooth->ramp_ms : 1;
	double ramp = fmax(RAMP_FLOOR, pow(fmin(progress, 1.0), smooth->ramp_power));
	// a glide whose total equals the notch: sum of v*dt with v decaying by
	// `decay` per second-fraction converges to v0 / -ln(decay per second)
	double k = -log(smooth->friction) * 240.0;
	a->velocity += delta * WHEEL_PIXELS_PER_DELTA * ramp * k;
	a->coasting = true;
	(void)delta_discrete;
	schedule(smooth);
	return true;
}

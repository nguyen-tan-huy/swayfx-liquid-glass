#include <assert.h>
#include <math.h>
#include <time.h>
#include "sway/animation_manager.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"

struct animation_manager {
	float tick_time;
	float progress_delta;
	int64_t last_ns;
	struct wl_event_source *tick;
	struct wl_list animations;
} animation_manager;

struct animation init_animation(void *data) {
	return (struct animation){
		.data = data,
		.progress = 0.0f,
		.multiplier = 0.0f,
		.velocity = 0.0f,
		.span = 0.0f,
		.next_span = 0.0f,
		.initialized = false,
		.closing = false,
		.update = NULL,
		.complete = NULL,
	};
}

static float ease_out_cubic(float p) {
	return pow(p - 1, 3) + 1;
}

static int64_t now_ns(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// swayctl: damped spring toward 1.0, advanced by dt seconds in closed form
// (stable at any dt). `response` is the period of the undamped motion, `bounce`
// 0 = critically damped, >0 overshoots (zeta = 1 - bounce). Returns true once at rest.
static bool spring_step(struct animation *a, float dt) {
	float response = MAX(config->animation_response_ms, 40.0f) / 1000.0f;
	float bounce = MIN(MAX(config->animation_bounce, 0.0f), 0.8f);
	if (a->closing) {
		response = MAX(response * config->animation_close_ratio, 0.04f);
		if (config->animation_close_bounce >= 0.0f) {
			bounce = MIN(config->animation_close_bounce, 0.8f);
		}
	}
	float w = 2.0f * 3.14159265f / response;
	float z = 1.0f - bounce;
	float x0 = a->multiplier - 1.0f, v0 = a->velocity, x, v;
	if (z < 0.999f) {
		float wd = w * sqrtf(1.0f - z * z);
		float e = expf(-z * w * dt), c = cosf(wd * dt), s = sinf(wd * dt);
		float b = (v0 + z * w * x0) / wd;
		x = e * (x0 * c + b * s);
		v = e * ((b * wd - z * w * x0) * c + (-x0 * wd - z * w * b) * s);
	} else {
		float e = expf(-w * dt), k = v0 + w * x0;
		x = e * (x0 + k * dt);
		v = e * (v0 - w * k * dt);
	}
	a->multiplier = 1.0f + x;
	a->velocity = v;
	a->progress = MIN(MAX(a->multiplier, 0.0f), 1.0f);
	if (fabsf(x) < 0.0008f && fabsf(v) < 0.02f) {
		a->multiplier = 1.0f;
		a->velocity = 0.0f;
		return true;
	}
	return false;
}

static int animation_timer() {
	int64_t now = now_ns();
	float dt = (float)(now - animation_manager.last_ns) / 1e9f;
	animation_manager.last_ns = now;
	dt = MIN(MAX(dt, 0.0005f), 0.05f);

	struct animation *animation, *tmp;
	wl_list_for_each_reverse_safe(animation, tmp, &animation_manager.animations, link) {
		bool done;
		if (config->animation_spring) {
			done = spring_step(animation, dt);
			if (done) {
				animation->progress = 1.0f;
			}
		} else {
			float delta = animation_manager.progress_delta / (animation->closing ? config->animation_close_ratio : 1.0f);
			animation->progress = MIN(animation->progress + delta, 1.0f);
			animation->multiplier = ease_out_cubic(animation->progress);
			done = animation->progress == 1.0f;
		}

		if (animation->update) {
			animation->update(animation->data);
		}

		if (done) {
			finish_animation(animation);
			if (animation->complete) {
				animation->complete(animation->data);
			}
		}
	}

	if (!wl_list_empty(&animation_manager.animations)) {
		wl_event_source_timer_update(animation_manager.tick,
				animation_manager.tick_time);
	}
	return 0;
}

void add_animation(struct animation *animation, void (*update_callback)(void *),
		void (*complete_callback)(void *)) {
	if (!config->animation_duration_ms) {
		if (complete_callback) {
			complete_callback(animation->data);
		}
		return;
	}
	// swayctl: a retarget while the spring is moving keeps its speed, rescaled
	// from the old segment's distance to the new one's
	float carried = 0.0f;
	if (config->animation_spring && animation->initialized &&
			animation->span > 0.5f && animation->next_span > 0.5f) {
		carried = animation->velocity * animation->span / animation->next_span;
	}
	animation->span = animation->next_span > 0.0f ? animation->next_span : 1.0f;
	animation->next_span = 0.0f;

	// remove previous instances of this animation
	if (animation->initialized) {
		wl_list_remove(&animation->link);
	}
	if (wl_list_empty(&animation_manager.animations)) {
		animation_manager.last_ns = now_ns();
	}

	animation->progress = 0.0f;
	animation->multiplier = 0.0f;
	animation->velocity = carried;
	animation->initialized = true;
	animation->update = update_callback;
	animation->complete = complete_callback;
	wl_list_insert(&animation_manager.animations, &animation->link);
}

void finish_animation(struct animation *animation) {
	if (animation->initialized) {
		wl_list_remove(&animation->link);
		animation->initialized = false;
	}
	animation->progress = 1.0f;
	animation->multiplier = 1.0f;
	animation->velocity = 0.0f;
	animation->closing = false;
}

void start_animations() {
	if (!config->animation_duration_ms) {
		return;
	}
	assert(animation_manager.tick);

	animation_manager.last_ns = now_ns();
	wl_event_source_timer_update(animation_manager.tick, 1);
}

static float get_fastest_output_refresh_ms() {
	float fastest_output_refresh_ms = 16.6667; // fallback to 60 Hz
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if (output->refresh_nsec > 0) {
			float output_refresh_ms = output->refresh_nsec / 1000000.0;
			fastest_output_refresh_ms = MIN(fastest_output_refresh_ms, output_refresh_ms);
		}
	}
	return fastest_output_refresh_ms;
}

void refresh_animation_manager_timing() {
	if (!config || !root) {
		return;
	}
	animation_manager.tick_time = get_fastest_output_refresh_ms();
	animation_manager.progress_delta = animation_manager.tick_time / config->animation_duration_ms;
}

void animation_manager_init(struct sway_server *server) {
	animation_manager.tick = wl_event_loop_add_timer(server->wl_event_loop,
			animation_timer, NULL);
	wl_list_init(&animation_manager.animations);
	refresh_animation_manager_timing();
}

static float lerp(float a, float b, float t) {
	return a * (1.0 - t) + b * t;
}

float get_animated_value(float from, float to, const struct animation *animation) {
	if (!config->animation_duration_ms || !animation->initialized) {
		return to;
	}
	return lerp(from, to, animation->multiplier);
}


float get_animated_alpha(float from, float to, const struct animation *animation) {
	if (!config->animation_duration_ms || !animation->initialized) {
		return to < 0.0f ? 0.0f : to > 1.0f ? 1.0f : to;
	}
	// opacity gets there ahead of the motion (animation_fade): the eye reads the fade first
	float t = animation->multiplier * config->animation_fade_gain;
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	float a = lerp(from, to, t);
	return a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
}

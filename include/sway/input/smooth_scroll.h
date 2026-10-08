#ifndef _SWAY_INPUT_SMOOTH_SCROLL_H
#define _SWAY_INPUT_SMOOTH_SCROLL_H
#include <stdbool.h>
#include <wlr/types/wlr_pointer.h>

struct sway_cursor;
struct sway_smooth_scroll;
struct input_config;

struct sway_smooth_scroll *smooth_scroll_create(struct sway_cursor *cursor);
void smooth_scroll_destroy(struct sway_smooth_scroll *smooth);

/**
 * Called where sway would send an axis event to the client under the cursor
 * (after bindings and title bar scrolling). Returns true when the event was
 * taken over: it will reach the client spread over the next frames, with
 * inertia after the fingers lift or the wheel stops.
 */
bool smooth_scroll_axis(struct sway_smooth_scroll *smooth, struct input_config *ic,
		struct wlr_pointer_axis_event *event, double delta, int32_t delta_discrete);

/** Drop any coasting, e.g. when the pointer moves to another surface. */
void smooth_scroll_stop(struct sway_smooth_scroll *smooth);

/** End a coast or wheel glide once the pointer has moved away from it. */
void smooth_scroll_motion(struct sway_smooth_scroll *smooth);

#endif

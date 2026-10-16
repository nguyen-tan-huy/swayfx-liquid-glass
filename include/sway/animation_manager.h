#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include <stdbool.h>
#include <wayland-util.h>

struct sway_container;
struct sway_server;

// TODO: make animation just a pointer to progress, make multiplier and callback private
struct animation {
	struct wl_list link;
	float progress;
	void *data;
	float multiplier;
	// swayctl: spring mode keeps the animation's own velocity (progress per
	// second) so a retarget mid-flight carries on instead of restarting.
	// `span` is how far (px, or 1 for a fade) the running segment travels;
	// callers set `next_span` before add_animation so the velocity can be
	// rescaled to the new segment.
	float velocity;
	float span;
	float next_span;
	bool initialized;
	void (*update)(void *);
	void (*complete)(void *);
};

void animation_manager_init(struct sway_server *server);

struct animation init_animation(void *data);

void refresh_animation_manager_timing();

void add_animation(struct animation *animation, void (*update_callback)(void *),
	void (*complete_callback)(void *));

void finish_animation(struct animation *animation);

void start_animations();

float get_animated_value(float from, float to, const struct animation *animation);

// like get_animated_value, clamped to 0..1: a spring overshoots, an opacity must not
float get_animated_alpha(float from, float to, const struct animation *animation);

#endif


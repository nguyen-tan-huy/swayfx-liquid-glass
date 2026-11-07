#ifndef _SWAY_LAYERS_H
#define _SWAY_LAYERS_H
#include <stdbool.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <scenefx/types/fx/glass.h>
#include "sway/layer_criteria.h"
#include "sway/animation_manager.h"
#include "sway/tree/view.h"

struct sway_layer_surface {
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener node_destroy;
	struct wl_listener new_popup;

	bool mapped;

	struct wlr_scene_tree *popups;
	struct sway_popup_desc desc;

	struct sway_output *output;

	struct wlr_scene_blur *blur_node;
	struct fx_backdrop_probe probe; // swayctl: what's behind it, for IPC_GET_BACKDROP
	struct wlr_scene_shadow *shadow_node;

	bool shadow_enabled;
	bool blur_enabled;
	bool blur_xray;
	bool blur_ignore_transparent;
	bool glass_enabled;   // swayctl
	int glass_refraction;
	float glass_highlight;
	int glass_blur;
	int glass_text;
	int glass_edge;     // 0 = auto from corner radius
	int glass_thickness; // px rim optical depth
	float glass_chroma; // fraction of refraction; -1 = auto (0.40); 0 = none
	int corner_radius;

	// swayctl: compositor-driven open/close (see layer_animate)
	bool animate;
	int anim_pivot_right, anim_pivot_top, anim_open_ms, anim_close_ms;
	float anim_bounce;
	struct animation anim;
	float anim_from, anim_to;   // the spring runs 0..1 between these (k = 1 is fully open)
	bool anim_shown;            // the nodes carry the animation's transform right now

	struct wl_list link; // sway_output.layer_surfaces

	struct wlr_scene_layer_surface_v1 *scene;
	struct wlr_scene_tree *tree;
	struct wlr_layer_surface_v1 *layer_surface;
};

struct sway_layer_popup {
	struct wlr_xdg_popup *wlr_popup;
	struct wlr_scene_tree *scene;
	struct sway_layer_surface *toplevel;

	struct wl_listener destroy;
	struct wl_listener new_popup;
	struct wl_listener commit;
	struct wl_listener reposition;
};

struct sway_output;

struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(
		struct wlr_surface *surface);

void arrange_layers(struct sway_output *output);

void layer_apply_criteria(struct sway_layer_surface *surface, struct layer_criteria *criteria);

void destroy_layers(struct sway_output *output);

// swayctl: open (true) or close (false) with the spring every surface of that namespace
// that asked for it; returns how many it moved
int layer_animate(const char *namespace, bool open);

// each repaint: scale, move and fade the surface (and what is drawn behind it) to where its
// spring is; once settled open, put everything back
void layer_animation_apply(struct sway_layer_surface *surface, struct wlr_scene_buffer *buffer);

#endif

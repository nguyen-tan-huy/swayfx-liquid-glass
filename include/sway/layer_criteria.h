#include <stdbool.h>

struct layer_criteria {
	char *namespace;
	char *cmdlist;

	bool shadow_enabled;
	bool blur_enabled;
	bool blur_xray;
	bool blur_ignore_transparent;
	int corner_radius;
	// swayctl: liquid glass over the blur
	bool glass_enabled;
	int glass_refraction;
	float glass_highlight; // rim/specular light, 0-1
	int glass_blur; // 0-100: how frosted; 0 = clear glass
	int glass_text; // text on it: 0 unknown, 1 light, 2 dark (the glass keeps it readable)
	int glass_edge;   // px bevel width; 0 = auto from corner radius
	int glass_thickness; // px rim optical depth (demo Glass Thickness)
	// swayctl: the compositor opens and closes the surface itself (a spring that scales it
	// out of a point, instead of the client redrawing every frame); see layer_animate
	bool animate;
	int anim_pivot_right; // px from the surface's right edge
	int anim_pivot_top;   // px from its top edge
	int anim_open_ms;     // spring period
	float anim_bounce;    // 0-0.8 when opening
	int anim_close_ms;
	// the look of the motion: it starts at min_scale (0 = out of a point), `drop` px above
	// its place, stretches with its speed by `stretch`, and is opaque once k reaches 1/fade
	float anim_min_scale;
	float anim_drop;
	float anim_stretch;
	float anim_fade;
	float glass_chroma; // color split at the rim as a fraction of refraction; -1 = auto (0.40); 0 = none
};

void layer_criteria_destroy(struct layer_criteria *criteria);

struct layer_criteria *layer_criteria_add(char *namespace, char *cmdlist);

// Get the matching criteria for a specified `sway_layer_surface`
struct layer_criteria *layer_criteria_for_namespace(char *namespace);

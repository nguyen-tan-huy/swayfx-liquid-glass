#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/layer_criteria.h"
#include "util.h"

static void init_criteria_effects(struct layer_criteria *criteria) {
	criteria->corner_radius = 0;
	criteria->blur_enabled = false;
	criteria->blur_xray = false;
	criteria->blur_ignore_transparent = false;
	criteria->shadow_enabled = false;
	criteria->glass_enabled = false;
	criteria->glass_refraction = 40;
	criteria->glass_highlight = 0.50f;
	criteria->glass_blur = 100;
	criteria->glass_text = 0;
	criteria->glass_edge = 0;
	criteria->glass_thickness = 0;
	criteria->glass_chroma = -1.0f; // auto: refraction * 0.40 (demo rim colour)
	criteria->animate = false;
	criteria->anim_min_scale = 0.0f;
	criteria->anim_drop = 0.0f;
	criteria->anim_stretch = 0.045f;
	criteria->anim_fade = 5.0f;
}

static void copy_criteria_effects(struct layer_criteria *dst, struct layer_criteria *src) {
	dst->corner_radius = src->corner_radius;
	dst->blur_enabled = src->blur_enabled;
	dst->blur_xray = src->blur_xray;
	dst->blur_ignore_transparent = src->blur_ignore_transparent;
	dst->shadow_enabled = src->shadow_enabled;
	dst->glass_enabled = src->glass_enabled;
	dst->glass_refraction = src->glass_refraction;
	dst->glass_highlight = src->glass_highlight;
	dst->glass_blur = src->glass_blur;
	dst->glass_text = src->glass_text;
	dst->glass_edge = src->glass_edge;
	dst->glass_thickness = src->glass_thickness;
	dst->glass_chroma = src->glass_chroma;
	dst->animate = src->animate;
	dst->anim_pivot_right = src->anim_pivot_right;
	dst->anim_pivot_top = src->anim_pivot_top;
	dst->anim_min_scale = src->anim_min_scale;
	dst->anim_drop = src->anim_drop;
	dst->anim_stretch = src->anim_stretch;
	dst->anim_fade = src->anim_fade;
}

static bool layer_criteria_find(char *namespace,
		struct layer_criteria **criteria, int *index) {
	*index = -1;
	*criteria = NULL;

	list_t *criterias = config->layer_criteria;
	for (int i = 0; i < criterias->length; ++i) {
		struct layer_criteria *existing = criterias->items[i];
		if (strcmp(namespace, existing->namespace) == 0) {
			*index = i;
			*criteria = existing;
			return true;
		}
	}
	return false;
}

static bool layer_criteria_parse(struct layer_criteria *criteria) {
	char matched_delim = ';';
	char *head = malloc(strlen(criteria->cmdlist) + 1);
	strcpy(head, criteria->cmdlist);
	do {
		// Trim leading whitespaces
		for (; isspace(*head); ++head) {}
		// Split command list
		char *cmd = argsep(&head, ";,", &matched_delim);
		for (; isspace(*cmd); ++cmd) {}

		if (strcmp(cmd, "") == 0) {
			sway_log(SWAY_INFO, "Ignoring empty layer effect.");
			continue;
		}
		sway_log(SWAY_INFO, "Handling layer effect '%s'", cmd);

		int argc;
		char **argv = split_args(cmd, &argc);
		// Strip all quotes from each token
		for (int i = 1; i < argc; ++i) {
			if (*argv[i] == '\"' || *argv[i] == '\'') {
				strip_quotes(argv[i]);
			}
		}
		if (strcmp(argv[0], "reset") == 0) {
			// Reset all args
			init_criteria_effects(criteria);
			return true;
		} else if (strcmp(argv[0], "blur") == 0) {
			criteria->blur_enabled = parse_boolean(argv[1], true);
			continue;
		} if (strcmp(argv[0], "blur_xray") == 0) {
			criteria->blur_xray = parse_boolean(argv[1], true);
			continue;
		} if (strcmp(argv[0], "blur_ignore_transparent") == 0) {
			criteria->blur_ignore_transparent = parse_boolean(argv[1], true);
			continue;
		} else if (strcmp(argv[0], "glass") == 0) {
			criteria->glass_enabled = parse_boolean(argv[1], true);
			continue;
		} else if (strcmp(argv[0], "glass_refraction") == 0 && argc > 1) {
			char *end;
			long px = strtol(argv[1], &end, 10);
			if (*end || px < 0 || px > 140) {
				sway_log(SWAY_ERROR, "Invalid glass_refraction (0-140 px): \"%s\"", argv[1]);
				return false;
			}
			criteria->glass_refraction = px;
			continue;
		} else if (strcmp(argv[0], "glass_blur") == 0 && argc > 1) {
			char *end;
			long b = strtol(argv[1], &end, 10);
			if (*end || b < 0 || b > 100) {
				sway_log(SWAY_ERROR, "Invalid glass_blur (0-100): \"%s\"", argv[1]);
				return false;
			}
			criteria->glass_blur = b;
			continue;
		} else if (strcmp(argv[0], "glass_text") == 0 && argc > 1) {
			if (strcmp(argv[1], "light") == 0) {
				criteria->glass_text = 1;
			} else if (strcmp(argv[1], "dark") == 0) {
				criteria->glass_text = 2;
			} else if (strcmp(argv[1], "none") == 0) {
				criteria->glass_text = 0;
			} else if (strcmp(argv[1], "auto") == 0) {
				criteria->glass_text = 3; // key ink, see commands/glass.c
			} else {
				sway_log(SWAY_ERROR, "Invalid glass_text (light|dark|auto|none): \"%s\"", argv[1]);
				return false;
			}
			continue;
		} else if (strcmp(argv[0], "glass_highlight") == 0 && argc > 1) {
			char *end;
			float h = strtof(argv[1], &end);
			if (*end || h < 0 || h > 1) {
				sway_log(SWAY_ERROR, "Invalid glass_highlight (0-1): \"%s\"", argv[1]);
				return false;
			}
			criteria->glass_highlight = h;
			continue;
		} else if (strcmp(argv[0], "glass_edge") == 0 && argc > 1) {
			char *end;
			long px = strtol(argv[1], &end, 10);
			if (*end || px < 0 || px > 140) {
				sway_log(SWAY_ERROR, "Invalid glass_edge (0-140 px): \"%s\"", argv[1]);
				return false;
			}
			criteria->glass_edge = px;
			continue;
		} else if (strcmp(argv[0], "glass_thickness") == 0 && argc > 1) {
			char *end;
			long px = strtol(argv[1], &end, 10);
			if (*end || px < 0 || px > 1500) {
				sway_log(SWAY_ERROR, "Invalid glass_thickness (0-1500 px): \"%s\"", argv[1]);
				return false;
			}
			criteria->glass_thickness = px;
			continue;
		} else if (strcmp(argv[0], "glass_chroma") == 0 && argc > 1) {
			char *end;
			float c = strtof(argv[1], &end);
			if (*end || c < 0 || c > 1) {
				sway_log(SWAY_ERROR, "Invalid glass_chroma (0-1): \"%s\"", argv[1]);
				return false;
			}
			criteria->glass_chroma = c;
			continue;
		} else if (strcmp(argv[0], "animate") == 0 && argc > 1) {
			// animate <on|off>
			// animate <pivot_right px, -1 = centred> <pivot_top px> [<min_scale 0-1> <drop px> <stretch 0-0.2> <fade 0.5-10>]
			// Only the look of the motion lives here; its pace and curve are the compositor's own
			// animation settings (animation_curve, animation_spring, animation_duration_ms), as for
			// windows, and `animations popups enable|disable` switches it.
			if (argc == 2) {
				criteria->animate = parse_boolean(argv[1], true);
				continue;
			}
			if (argc != 3 && argc != 7) {
				sway_log(SWAY_ERROR, "animate takes <right px> <top px> [<min scale> <drop px> <stretch> <fade>]");
				return false;
			}
			char *e[6];
			long right = strtol(argv[1], &e[0], 10), top = strtol(argv[2], &e[1], 10);
			float min_scale = 0.0f, drop = 0.0f, stretch = 0.045f, fade = 5.0f;
			bool ok = !*e[0] && !*e[1] && right >= -1 && right <= 4000 && top >= 0 && top <= 4000;
			if (ok && argc == 7) {
				min_scale = strtof(argv[3], &e[2]);
				drop = strtof(argv[4], &e[3]);
				stretch = strtof(argv[5], &e[4]);
				fade = strtof(argv[6], &e[5]);
				ok = !*e[2] && !*e[3] && !*e[4] && !*e[5] && min_scale >= 0 && min_scale <= 1 &&
					drop >= 0 && drop <= 400 && stretch >= 0 && stretch <= 0.2f && fade >= 0.5f && fade <= 10;
			}
			if (!ok) {
				sway_log(SWAY_ERROR, "Invalid animate arguments");
				return false;
			}
			criteria->animate = true;
			criteria->anim_pivot_right = right;
			criteria->anim_pivot_top = top;
			criteria->anim_min_scale = min_scale;
			criteria->anim_drop = drop;
			criteria->anim_stretch = stretch;
			criteria->anim_fade = fade;
			continue;
		} else if (strcmp(argv[0], "shadows") == 0) {
			criteria->shadow_enabled = parse_boolean(argv[1], true);
			continue;
		} else if (strcmp(argv[0], "corner_radius") == 0) {
			int value;
			if (cmd_corner_radius_parse_value(argv[1], &value)) {
				criteria->corner_radius = value;
				continue;
			}
			sway_log(SWAY_ERROR,
					"Invalid layer_effects corner_radius size! Got \"%s\"",
					argv[1]);
			return false;
		} else {
			sway_log(SWAY_ERROR, "Invalid layer_effects effect! Got \"%s\"", cmd);
			return false;
		}
	} while(head);

	return true;
}

struct layer_criteria *layer_criteria_add(char *namespace, char *cmdlist) {
	struct layer_criteria *criteria = calloc(1, sizeof(*criteria));
	if (!criteria) {
		sway_log(SWAY_ERROR, "Could not allocate new layer_criteria");
		return NULL;
	}
	init_criteria_effects(criteria);

	// Start from the existing rule's effects, if any
	int existing_index;
	struct layer_criteria *existing = NULL;
	bool replace = layer_criteria_find(namespace, &existing, &existing_index);
	if (replace) {
		copy_criteria_effects(criteria, existing);
	}
	criteria->namespace = malloc(strlen(namespace) + 1);
	strcpy(criteria->namespace, namespace);
	criteria->cmdlist = cmdlist;

	// Parsing. swayctl: only drop the old rule once the new one parsed -- a
	// bad effect (e.g. an out-of-range corner_radius) used to delete every
	// effect set on the namespace before it.
	if (!layer_criteria_parse(criteria)) {
		layer_criteria_destroy(criteria);
		return NULL;
	}
	if (replace) {
		sway_log(SWAY_DEBUG, "layer_effect '%s' already exists: '%s'. Replacing with: '%s'",
				namespace, existing->cmdlist, cmdlist);
		layer_criteria_destroy(existing);
		list_del(config->layer_criteria, existing_index);
	}

	list_add(config->layer_criteria, criteria);

	return criteria;
}

struct layer_criteria *layer_criteria_for_namespace(char *namespace) {
	list_t *criterias = config->layer_criteria;
	for (int i = criterias->length - 1; i >= 0; i--) {
		struct layer_criteria *criteria = criterias->items[i];
		if (strcmp(criteria->namespace, namespace) == 0) {
			return criteria;
		}
	}
	return NULL;
}

void layer_criteria_destroy(struct layer_criteria *criteria) {
	free(criteria->namespace);
	free(criteria->cmdlist);
	free(criteria);
}

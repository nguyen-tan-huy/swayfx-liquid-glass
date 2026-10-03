// swayctl: liquid glass behind a window (the same SceneFX glass as
// layer_effects "glass"), for for_window / criteria:
//   for_window [app_id="..."] glass enable|disable
//   for_window [app_id="..."] glass refraction <0-120 px>
//   for_window [app_id="..."] glass blur <0-100>      (0 = clear glass)
//   for_window [app_id="..."] glass text light|dark|none  (keeps that text readable)
// The glass takes the shape of what the window draws (any alpha counts), so a
// window with a transparent background gets a pane of glass per widget.
#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/root.h"
#include "util.h"

struct cmd_results *cmd_glass(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "glass", EXPECTED_AT_LEAST, 1);
	if (error) {
		return error;
	}
	struct sway_container *con = config->handler_context.container;
	if (con == NULL || con->view == NULL) {
		return cmd_results_new(CMD_FAILURE, "glass: needs a window (use it with for_window or [criteria])");
	}
	if (strcmp(argv[0], "text") == 0) {
		if (argc < 2) {
			return cmd_results_new(CMD_INVALID, "glass text: light|dark|none");
		}
		if (strcmp(argv[1], "light") == 0) {
			con->glass_text = 1;
		} else if (strcmp(argv[1], "dark") == 0) {
			con->glass_text = 2;
		} else if (strcmp(argv[1], "none") == 0) {
			con->glass_text = 0;
		} else {
			return cmd_results_new(CMD_INVALID, "glass text: light|dark|none");
		}
	} else if (strcmp(argv[0], "refraction") == 0 || strcmp(argv[0], "blur") == 0) {
		if (argc < 2) {
			return cmd_results_new(CMD_INVALID, "glass %s: missing value", argv[0]);
		}
		char *end;
		long v = strtol(argv[1], &end, 10);
		long max = strcmp(argv[0], "blur") == 0 ? 100 : 120;
		if (*end || v < 0 || v > max) {
			return cmd_results_new(CMD_INVALID, "glass %s: 0-%ld", argv[0], max);
		}
		if (strcmp(argv[0], "blur") == 0) {
			con->glass_blur = v;
		} else {
			con->glass_refraction = v;
		}
	} else {
		con->glass_enabled = parse_boolean(argv[0], con->glass_enabled);
		// glass is drawn by the blur node
		if (con->glass_enabled) {
			con->blur_enabled = true;
		} else {
			con->blur_enabled = config->blur_enabled;
		}
	}
	arrange_container(con);
	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		wlr_scene_optimized_blur_mark_dirty(output->layers.blur_layer);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

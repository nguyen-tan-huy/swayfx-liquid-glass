// swayctl DEBUG: glass_debug <flags> - turn glass optimizations off one by
// one on a live session (see fx_glass_debug in scenefx/types/fx/glass.h)
#include <stdlib.h>
#include "scenefx/types/fx/glass.h"
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/root.h"

struct cmd_results *cmd_glass_debug(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "glass_debug", EXPECTED_EQUAL_TO, 1);
	if (error) {
		return error;
	}
	fx_glass_debug = (int)strtol(argv[0], NULL, 0);
	// enabled outputs only (all_outputs also holds the headless fallback)
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		wlr_scene_optimized_blur_mark_dirty(output->layers.blur_layer);
		if (output->scene_output) {
			wlr_damage_ring_add_whole(&output->scene_output->damage_ring);
		}
		wlr_output_schedule_frame(output->wlr_output);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

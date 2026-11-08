// swayctl patch: open or close, with the compositor's spring, the layer surfaces of a namespace
// that asked for it (layer_effects <namespace> animate ...).
//   layer_animate <namespace> open|close
#include <strings.h>
#include "sway/commands.h"
#include "sway/layers.h"

struct cmd_results *cmd_layer_animate(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layer_animate", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}
	bool open;
	if (strcasecmp(argv[1], "open") == 0) {
		open = true;
	} else if (strcasecmp(argv[1], "close") == 0) {
		open = false;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected 'layer_animate <namespace> open|close'");
	}
	// failing tells the client nothing is moving (animations are off, or no such popup is open):
	// it must not wait for an animation that will never run
	int settle_ms = layer_animate(argv[0], open);
	if (settle_ms == 0) {
		return cmd_results_new(CMD_FAILURE, "no layer surface is animating");
	}
	// the client closes its window once the motion is over: it is told how long that takes
	// (the animation settings are the compositor's, not the client's)
	return cmd_results_new(CMD_SUCCESS, "%d", settle_ms);
}

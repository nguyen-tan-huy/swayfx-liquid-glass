// swayctl patch: turn each kind of animation on or off on its own; the
// duration (animation_duration_ms) still sets them all, 0 = none.
//   animations open_close|workspace|move enable|disable
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *cmd_animations(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "animations", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}
	bool *flag;
	if (strcasecmp(argv[0], "open_close") == 0) {
		flag = &config->animate_open_close;
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		flag = &config->animate_workspace;
	} else if (strcasecmp(argv[0], "move") == 0) {
		flag = &config->animate_move;
	} else {
		return cmd_results_new(CMD_INVALID,
			"Expected 'animations open_close|workspace|move enable|disable'");
	}
	*flag = parse_boolean(argv[1], *flag);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// swayctl: `scroll_native enable|disable` on a window, e.g.
//   for_window [app_id="firefox"] scroll_native enable
// sends it the raw scroll events, for apps that smooth scrolling themselves.
#include "sway/commands.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/input/smooth_scroll.h"
#include "sway/tree/container.h"
#include "util.h"

struct cmd_results *cmd_scroll_native(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_native", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct sway_container *con = config->handler_context.container;
	if (!con) {
		return cmd_results_new(CMD_FAILURE, "No current container");
	}
	con->scroll_native = parse_boolean(argv[0], con->scroll_native);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// swayctl: `scroll_native enable|disable` on a window, e.g.
//   for_window [app_id="firefox"] scroll_native enable
// sends it the raw scroll events, for apps that smooth scrolling themselves.
#include <stdlib.h>
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

// `scroll_scale <factor>` on a window, e.g.
//   for_window [app_id="google-chrome"] scroll_scale 0.5
// scales the smooth scrolling that window gets (Chromium turns the compositor's
// pixel deltas into a longer scroll than GTK apps do).
struct cmd_results *cmd_scroll_scale(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_scale", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct sway_container *con = config->handler_context.container;
	if (!con) {
		return cmd_results_new(CMD_FAILURE, "No current container");
	}
	char *end;
	float f = strtof(argv[0], &end);
	if (*end != '\0' || f <= 0 || f > 10) {
		return cmd_results_new(CMD_INVALID, "scroll_scale takes a number above 0, up to 10");
	}
	con->scroll_scale = f;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// `scroll_page_keys enable|disable` on a window, e.g.
//   for_window [app_id="google-chrome"] scroll_page_keys enable
// Page Up / Page Down then glide the page like a wheel instead of jumping.
struct cmd_results *cmd_scroll_page_keys(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_page_keys", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct sway_container *con = config->handler_context.container;
	if (!con) {
		return cmd_results_new(CMD_FAILURE, "No current container");
	}
	con->scroll_page_keys = parse_boolean(argv[0], con->scroll_page_keys);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

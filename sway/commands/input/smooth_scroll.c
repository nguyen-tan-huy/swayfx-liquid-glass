// swayctl: compositor smooth scrolling settings (see sway/input/smooth_scroll.c)
#include <math.h>
#include <string.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "util.h"

static struct input_config *current(struct cmd_results **error) {
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		*error = cmd_results_new(CMD_FAILURE, "No input device defined.");
	}
	return ic;
}

struct cmd_results *input_cmd_smooth_scroll(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "smooth_scroll", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = current(&error);
	if (!ic) {
		return error;
	}
	ic->smooth_scroll = parse_boolean(argv[0], true) ? 1 : 0;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *input_cmd_scroll_friction(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_friction", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = current(&error);
	if (!ic) {
		return error;
	}
	float f = parse_float(argv[0]);
	if (isnan(f) || f <= 0 || f >= 1) {
		return cmd_results_new(CMD_INVALID,
			"scroll_friction: expected a number between 0 and 1 (per 1/240 s; higher slides longer)");
	}
	ic->scroll_friction = f;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *input_cmd_scroll_ramp(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_ramp", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}
	struct input_config *ic = current(&error);
	if (!ic) {
		return error;
	}
	float ms = parse_float(argv[0]), power = parse_float(argv[1]);
	if (isnan(ms) || ms < 0 || ms > 5000 || isnan(power) || power <= 0 || power > 10) {
		return cmd_results_new(CMD_INVALID, "scroll_ramp: expected <ms 0-5000> <power 0-10>");
	}
	ic->scroll_ramp_ms = ms;
	ic->scroll_ramp_power = power;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// swayctl patch: spring animations.
//   animation_curve ease|spring            fixed ease-out-cubic (upstream) or a spring
//   animation_spring <response_ms> [bounce]  spring period (40..2000) and bounce (0..0.8)
// animation_duration_ms still switches every animation on or off (0 = none); with
// the spring the duration is only that switch, the response sets the pace.
#include <stdlib.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_animation_curve(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "animation_curve", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "spring") == 0) {
		config->animation_spring = true;
	} else if (strcasecmp(argv[0], "ease") == 0) {
		config->animation_spring = false;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected 'animation_curve ease|spring'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_animation_spring(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "animation_spring", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	char *end;
	float response = strtof(argv[0], &end);
	if (*end || response < 40 || response > 2000) {
		return cmd_results_new(CMD_INVALID, "animation_spring response must be 40..2000 ms");
	}
	float bounce = config->animation_bounce;
	if (argc > 1) {
		bounce = strtof(argv[1], &end);
		if (*end || bounce < 0 || bounce > 0.8f) {
			return cmd_results_new(CMD_INVALID, "animation_spring bounce must be 0..0.8");
		}
	}
	config->animation_response_ms = response;
	config->animation_bounce = bounce;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

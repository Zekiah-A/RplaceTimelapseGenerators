#include <png.h>
#include <pthread.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <argp.h>
#include "console.h"
#include "main_thread.h"

const char* argp_program_version = "NativeTimelapseGenerator 1.0";
const char* argp_program_bug_address = "<bug@example.com>";
static char doc[] = "NativeTimelapseGenerator Generator -- A program to generate timelapses from rplace canvas data";
static char args_doc[] = "";
static struct argp_option options[] = {
	{"nocli", 'n', 0, 0, "Disable CLI"},
	{"repo", 'r', "URL", 0, "Repository URL"},
	{"log", 'l', "FILE", 0, "Log file name"},
	{0}
};
struct arguments {
	bool no_cli;
	const char* repo_url;
	const char* log_file_name;
};

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
	struct arguments* arguments = state->input;

	switch (key) {
	case 'n':
		arguments->no_cli = true;
		break;
	case 'r':
		arguments->repo_url = arg;
		break;
	case 'l':
		arguments->log_file_name = arg;
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num >= 0) {
			argp_usage(state);
		}
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 0) {
			argp_usage(state);
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = args_doc,
	.doc = doc
};

int main(int argc, char *argv[]) {
	struct arguments arguments = {
		.no_cli = false,
		.repo_url = "https://github.com/rslashplace2/rslashplace2.github.io",
		.log_file_name = NULL
	};

	argp_parse(&argp, argc, argv, 0, 0, &arguments);
	if (!arguments.no_cli) {
		// Start console thread
		pthread_t thread_id;
		pthread_create(&thread_id, NULL, start_console, NULL);
	}

	// Start main thread (will never return, and handle exiting itself)
	start_main_thread(arguments.no_cli, arguments.repo_url, arguments.log_file_name);
	return 0;
}
// ffmpeg -framerate 24 -pattern_type glob -i "backups/*.png" -c:v libx264 -pix_fmt yuv420p -vf "pad=2000:2000:(ow-iw)/2:(oh-ih)/2" timelapse.mp4

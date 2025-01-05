#include <png.h>
#include <pthread.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <stdlib.h>
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
	{"no-cli", 'n', 0, 0, "Disable CLI"},
	{"repo-url", 'r', "URL", 0, "Repository URL"},
	{"commit-hashes-file", 'c', "FILE", 0, "Commit hashes file path"},
	{"download-root-url", 'd', "URL", 0, "Download root URL"},
	{"game-server-root-url", 's', "URL", 0, "Socket server root URL (HTTP)"},
	{"max-top-placers", 'p', "NUMBER", 0, "Max top placers listed"},
	{0}
};

struct arguments {
	Config;
	bool no_cli;
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
		case 'd':
			arguments->download_base_url = arg;
			break;
		case 'c':
			arguments->commit_hashes_file_name = arg;
			break;
		case 's':
			arguments->game_server_base_url = arg;
			break;
		case 'p':
			arguments->max_top_placers = atoi(arg);
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
		.repo_url = NULL,
		.download_base_url = "https://raw.githubusercontent.com/rplacetk/canvas1",
		.game_server_base_url = "https://server.rplace.live",
		.commit_hashes_file_name = NULL,
		.max_top_placers = 10,
		.no_cli = false
	};

	argp_parse(&argp, argc, argv, 0, 0, &arguments);
	if (!arguments.no_cli) {
		// Start console thread
		pthread_t thread_id;
		pthread_create(&thread_id, NULL, start_console, NULL);
	}

	// Start main thread (will never return, and handle exiting itself)
	start_main_thread(arguments.no_cli, *(Config*) &arguments);
	return 0;
}
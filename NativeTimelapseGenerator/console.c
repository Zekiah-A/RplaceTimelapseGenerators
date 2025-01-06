#include <linux/limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include "console.h"
#include "main_thread.h"
#include "memory_utils.h"

// NAME: start_console, ARGS: start_generation_callback, stop_generation_callback, add_worker_callback remove_worker_callback
typedef void (*start_console_delegate)(void(*)(char*, char*, char*), void (*)(void), void (*)(int, int), void (*)(int, int));
// NAME: log_message, ARGS: char* message
typedef void (*log_message_delegate)(char*);
// NAME: stop_console
typedef void (*stop_console_delegate)();
// NAME: update_worker_stats
typedef void (*update_worker_stats_delegate)(int, int);
// NAME: update_backups_stats
typedef void (*update_backups_delegate)(int, float, CanvasInfo);

void* handle = NULL;
start_console_delegate start_console_cli = NULL;
log_message_delegate log_message_cli = NULL;
stop_console_delegate stop_console_cli = NULL;
update_worker_stats_delegate update_worker_stats_cli = NULL;
update_backups_delegate update_backups_stats_cli = NULL;

// TODO: For some reason the dbg so doesn't contain any exports?
//#ifdef DEBUG
//#define CONSOLE_LIB_NAME "NativeTimelapseGenerator.Console.so.dbg"
//#else
#define CONSOLE_LIB_NAME "NativeTimelapseGenerator.Console.so"
//#endif

// We still in UI thread, must pass back to main thread
void ui_start_generation(char* download_base_url, char* repo_url, char* log_file_name)
{
	main_thread_post((MainThreadWork) {
		.func = (WorkFunction)start_generation,
		.arg_count = 2,
		.args = { download_base_url, repo_url, log_file_name }
	});
}

// We still in UI thread, must pass back to main thread
void ui_stop_generation()
{
	printf("Generator CLI application halted. Application will terminate immediately\n");
	main_thread_post((MainThreadWork) { .func = (WorkFunction)stop_generation });
}

WorkFunction add_funcs[3] = { (WorkFunction)add_download_worker, (WorkFunction)add_render_worker, (WorkFunction)add_save_worker };
WorkFunction remove_funcs[3] = { (WorkFunction)remove_download_worker, (WorkFunction)remove_render_worker, (WorkFunction)remove_save_worker };

void ui_add_worker(int worker_type, int add)
{
	for (int i = 0; i < add; i++) {
		main_thread_post((MainThreadWork) { .func = add_funcs[worker_type] });
	}
}

void ui_remove_worker(int worker_type, int remove)
{
	for (int i = 0; i < remove; i++) {
		main_thread_post((MainThreadWork) { .func = remove_funcs[worker_type] });
	}
}

void* start_console(void* _)
{
	char lib_path[PATH_MAX];
	getcwd(lib_path, sizeof(lib_path));
	if (lib_path[strlen(lib_path) - 1] != '/') {
		strcat(lib_path, "/");
	}
	strcat(lib_path, CONSOLE_LIB_NAME);

	handle = dlopen(lib_path, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "Error - Unable to load library: %s\n", dlerror());
		exit(EXIT_FAILURE);
	}
	start_console_cli = dlsym(handle, "start_console");
	if (!start_console_cli) {
		fprintf(stderr, "Error - Unable to find 'start_console' symbol: %s\n", dlerror());
	}
	log_message_cli = dlsym(handle, "log_message");
	if (!log_message_cli) {
		fprintf(stderr, "Error - Unable to find 'log_message' symbol: %s\n", dlerror());
	}
	stop_console_cli = dlsym(handle, "stop_console");
	if (!stop_console_cli) {
		fprintf(stderr, "Error - Unable to find 'stop_console' symbol: %s\n", dlerror());
	}
	update_worker_stats_cli = dlsym(handle, "update_worker_stats");
	if (!update_worker_stats_cli) {
		fprintf(stderr, "Error - Unable to find 'update_worker_stats' symbol: %s\n", dlerror());
	}
	update_backups_stats_cli = dlsym(handle, "update_backups_stats");
	if (!update_backups_stats_cli) {
		fprintf(stderr, "Error - Unable to find 'update_backups_stats' symbol: %s\n", dlerror());
	}

	start_console_cli(&ui_start_generation, &ui_stop_generation, &ui_add_worker, &ui_remove_worker);
	return NULL;
}

void stop_console()
{
	if (stop_console_cli != NULL) {
		stop_console_cli();
	}
	else {
		fprintf(stderr, "Error - Could not stop console CLI. Method was null\n");
	}
}

void log_message(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	size_t size = vsnprintf(NULL, 0, format, args) + 1; // +1 for null-terminator
	va_end(args);

	char* buffer = (char*) malloc(size);
	va_start(args, format);
	vsnprintf(buffer, size, format, args);
	va_end(args);

	if (log_message_cli != NULL) {
		log_message_cli(buffer);
	}
	else {
		AUTOFREE char* print = NULL;
		asprintf(&print, "log_message: %s", buffer);
		puts(print);
	}

	free(buffer);
}

void update_worker_stats(WorkerStep worker_step, int count)
{
	if (update_worker_stats_cli != NULL) {
		update_worker_stats_cli(worker_step, count);
	}
	else {
		AUTOFREE char* print = NULL;
		asprintf(&print, "worker_stats: type: %d count: %d", worker_step, count);
		puts(print);
	}
}

void update_backups_stats(int backups_total, float backups_per_second, CanvasInfo current_info)
{
	if (update_backups_stats_cli != NULL) {
		update_backups_stats_cli(backups_total, backups_per_second, current_info);
	}
	else {
		AUTOFREE char* print = NULL;
		asprintf(&print, "backups_stats: generated: %d per second: %f processing: %li", backups_total, backups_per_second, current_info.date);
		puts(print);
	}
}
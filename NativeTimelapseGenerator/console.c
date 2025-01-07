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

WorkFunction add_funcs[3] = { (WorkFunction)add_download_worker, (WorkFunction)add_render_worker, (WorkFunction)add_save_worker };
WorkFunction remove_funcs[3] = { (WorkFunction)remove_download_worker, (WorkFunction)remove_render_worker, (WorkFunction)remove_save_worker };

// Called by UI thread, must pass back to main thread
void ui_start_generation(char* download_base_url, char* repo_url, char* log_file_name)
{
	main_thread_post((MainThreadWork) {
		.func = (WorkFunction)start_generation,
		.arg_count = 2,
		.args = { download_base_url, repo_url, log_file_name }
	});
}

// Called by UI thread, must pass back to main thread
void ui_stop_generation()
{
	printf("Generator CLI application halted. Application will terminate immediately\n");
	main_thread_post((MainThreadWork) { .func = (WorkFunction)stop_generation });
}

// Called by UI thread, must pass back to main thread
void ui_add_worker(int worker_type, int add)
{
	for (int i = 0; i < add; i++) {
		main_thread_post((MainThreadWork) { .func = add_funcs[worker_type] });
	}
}

// Called by UI thread, must pass back to main thread
void ui_remove_worker(int worker_type, int remove)
{
	for (int i = 0; i < remove; i++) {
		main_thread_post((MainThreadWork) { .func = remove_funcs[worker_type] });
	}
}

void* start_console(void* _)
{
	// TODO: Start webserver and open browser (if found)
	return NULL;
}

void stop_console()
{
	// TODO: Stop webserver and disconnect any sockets
}

void log_message(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	size_t size = vsnprintf(NULL, 0, format, args) + 1; // +1 for null-terminator
	va_end(args);

	AUTOFREE char* buffer = (char*) malloc(size);
	va_start(args, format);
	vsnprintf(buffer, size, format, args);
	va_end(args);

	// TODO: Send websocket update

	AUTOFREE char* print = NULL;
	asprintf(&print, "log_message: %s", buffer);
	puts(print);
}

void update_worker_stats(WorkerStep worker_step, int count)
{
	// TODO: Send websocket update

	AUTOFREE char* print = NULL;
	asprintf(&print, "worker_stats: type: %d count: %d", worker_step, count);
	puts(print);
}

void update_backups_stats(int backups_total, float backups_per_second, CanvasInfo current_info)
{
	// TODO: Send websocket update

	AUTOFREE char* print = NULL;
	asprintf(&print, "backups_stats: generated: %d per second: %f processing: %li", backups_total, backups_per_second, current_info.date);
	puts(print);
}
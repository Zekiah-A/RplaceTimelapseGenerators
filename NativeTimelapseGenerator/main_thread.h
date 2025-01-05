#pragma once
#include <bits/pthreadtypes.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdbool.h>

#include "workers/worker_structs.h"

typedef void* WorkFunction;
typedef union {
	void* args[8];
} WorkFunctionArgs;

typedef struct main_thread_work
{
    WorkFunction func;
	int arg_count;
	WorkFunctionArgs args;
} MainThreadWork;

typedef struct main_thread_queue {
	struct main_thread_work* work;
	size_t capacity;
	size_t front;
	size_t rear;
	pthread_mutex_t mutex;
} MainThreadQueue;

typedef struct config {
	const char* repo_url;
	const char* download_base_url;
	const char* commit_hashes_file_name;
	const char* game_server_base_url;
	int max_top_placers;
} Config;

// Generic thread data for each worker
typedef struct worker_info {
	// Global
	const Config* config;
	// Per thread / worker
	pthread_t thread_id;
	int worker_id;
	union
	{
		CanvasInfo* current_canvas_info;
		DownloadResult* current_download_result;
		RenderResult* current_render_result;
	};
	// Shared between workers
	union
	{
		struct download_worker_data* download_worker_data;
		struct render_worker_data* render_worker_data;
		struct save_worker_data* save_worker_data;
	};
} WorkerInfo;

typedef enum worker_type {
	DOWNLOAD_WORKER_TYPE = 0,
	RENDER_WORKER_TYPE = 1,
	SAVE_WORKER_TYPE = 2
} WorkerType;

typedef struct stack {
	void* items;
	size_t item_size;
	int top;
	int max_size;
	pthread_mutex_t mutex;
	bool replenished;
} Stack;
void init_stack(Stack* stack, size_t item_size, int max_size);
void push_stack(Stack* stack, void* item);
int pop_stack(Stack* stack, void* item);
void free_stack(Stack* stack);

void terminate_and_cleanup_workers(WorkerInfo** workers, int* worker_top, WorkerType worker_type);

// STRICT: Call from main thread only
void start_main_thread(bool start, Config config);

void main_thread_post(MainThreadWork work);
// STRICT: Call on main thread only - POST
void start_generation(Config config);
const char* clone_and_log_repo(const char* repo_url);
// STRICT: Call on main thread only - POST
void stop_generation();
// STRICT: Call on main thread only - POST
void add_download_worker();
// STRICT: Call on main thread only - POST
void remove_download_worker();
// STRICT: Call on main thread only - POST
void add_render_worker();
// STRICT: Call on main thread only - POST
void remove_render_worker();
// STRICT: Call on main thread only - POST
void add_save_worker();
// STRICT: Call on main thread only - POST
void remove_save_worker();
// STRICT: Call on main thread only - POST
void collect_backup_stats();

// Called by download worker
CanvasInfo pop_download_stack(int worker_id);
void push_render_stack(DownloadResult result);
// Called by render worker
DownloadResult pop_render_stack(int worker_id);
void push_save_stack(RenderResult result);
// Called by save worker
RenderResult pop_save_stack(int worker_id);
void push_completed(SaveResult result);
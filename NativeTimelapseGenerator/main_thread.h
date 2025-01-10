#pragma once
#include <bits/pthreadtypes.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdbool.h>
#include <avcall.h>

#include "workers/download_worker.h"
#include "workers/render_worker.h"
#include "workers/save_worker.h"
#include "workers/worker_structs.h"

typedef enum worker_status:uint8_t {
	WORKER_STATUS_WAITING = 0,
	WORKER_STATUS_ACTIVE = 1
} WorkerStatus;

typedef struct main_thread_queue {
	av_alist* work;
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
	// Per thread / worker
	int worker_id;
	pthread_t thread_id;
	WorkerStatus status;
	union
	{
		DownloadWorkerInstance* download_worker_instance;
		RenderWorkerInstance* render_worker_instance;
		SaveWorkerInstance* save_worker_instance;
	};

	// Shared between workers (global)
	const Config* config;
	union
	{
		const DownloadWorkerShared* download_worker_shared;
		const RenderWorkerShared* render_worker_shared;
		const SaveWorkerShared* save_worker_shared;
	};
} WorkerInfo;

typedef enum worker_type:uint8_t {
	WORKER_TYPE_DOWNLOAD = 0,
	WORKER_TYPE_RENDER = 1,
	WORKER_TYPE_SAVE = 2
} WorkerType;

void main_thread_post(av_alist work);

// STRICT: Call from main thread only
void start_main_thread(bool start, Config config);
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

// Better to call on main thread but shouldn't cause issues otherwise
WorkerInfo** get_workers(WorkerType type);

// Called by download worker
CanvasInfo pop_download_stack(int worker_id);
void push_render_stack(DownloadResult result);
// Called by render worker
DownloadResult pop_render_stack(int worker_id);
void push_save_stack(RenderResult result);
// Called by save worker
RenderResult pop_save_stack(int worker_id);
void push_completed(SaveResult result);
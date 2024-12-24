#pragma once
#include "worker_structs.h"

typedef enum worker_step {
	WORKER_STEP_DOWNLOAD = 0,
	WORKER_STEP_RENDER = 1,
	WORKER_STEP_SAVE = 2
} WorkerStep;

void update_worker_stats(WorkerStep worker_step, int count);
void update_backups_stats(int backups_total, float backups_per_second, CanvasInfo current_info);
void log_message(const char* format, ...);
void* start_console(void* data);
void stop_console();
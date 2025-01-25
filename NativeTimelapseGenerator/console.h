#pragma once
#include "workers/worker_structs.h"
#include "main_thread.h"

typedef enum log_type {
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR
} LogType;

void update_worker_stats(WorkerType worker_type, int count);
void update_save_stats(int completed_saves, float saves_per_second, SaveResult* save_results);
void log_message(LogType type, const char* format, ...);
void start_console();
void stop_console();
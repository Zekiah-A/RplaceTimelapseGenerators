#pragma once
#include "workers/worker_structs.h"
#include "main_thread.h"

typedef enum log_type {
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR
} LogType;

void update_worker_stats(WorkerType worker_type, int count);
void update_backups_stats(int backups_total, float backups_per_second, CommitInfo current_info);
void log_message(LogType type, const char* format, ...);
void start_console();
void stop_console();
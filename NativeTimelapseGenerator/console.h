#pragma once
#include "worker-structs.h"
void update_worker_stats(int worker_step, int count);
void update_backups_stats(int backups_total, int backups_per_second, struct canvas_info current_info);
void log_message(const char* format, ...);
void* start_console(void* data);
void stop_console();

#define WORKER_STEP_DOWNLOAD 0
#define WORKER_STEP_RENDER 1
#define WORKER_STEP_SAVE 2
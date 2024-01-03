#pragma once
void update_worker_stats(int step, int count);
void log_message(const char* format, ...);
void* start_console(void* data);
void stop_console();

#define WORKER_STEP_DOWNLOAD 0
#define WORKER_STEP_RENDER 1
#define WORKER_STEP_SAVE 2

// NAME: start_console, ARGS: start_generation_callback, stop_generation_callback
typedef void (*start_console_delegate)(void*, void*);
// NAME: log_message, ARGS: char* message
typedef void (*log_message_delegate)(char*);
//NAME: stop_console
typedef void (*stop_console_delegate)();
#pragma once
#include <bits/pthreadtypes.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "canvas-saver.h"
#include <stdbool.h>

// Call from main thread only
void start_main_thread(bool start_generation);

typedef void (*WorkFunction)(void*);

struct main_thread_work
{
    WorkFunction func;
    void* data;
};

struct main_thread_queue
{
    struct main_thread_work* work;
    size_t capacity;
    size_t front;
    size_t rear;
    pthread_mutex_t mutex;
};

// Generic thread data for each worker
typedef struct worker_info
{
    pthread_t thread_id;
    int worker_id;
    union
    {
        struct download_worker_data* download_worker_data;
        struct render_worker_data* render_worker_data;
        struct save_worker_data* save_worker_data;
    };
} WorkerInfo;

void main_thread_post(struct main_thread_work work);
// STRICT: Call on main thread only - POST
void start_generation();
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
struct canvas_info pop_download_stack(int worker_id);
void push_render_stack(struct downloaded_result result);
// Called by render worker
struct downloaded_result pop_render_stack(int worker_id);
void push_save_stack(struct render_result result);
// Called by save worker
struct render_result pop_save_stack(int worker_id);
void push_completed_frame(struct canvas_info info);
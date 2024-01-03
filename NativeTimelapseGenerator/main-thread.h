#pragma once
#include <bits/pthreadtypes.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "image-generator.h"
#include "canvas-saver.h"
#include "canvas-downloader.h"
#include "worker-structs.h"

// Call from main thread only
void start_main_thread();

struct main_thread_work
{
    void (*func)(void*);
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
    union
    {
        struct download_worker_data* download_worker_data;
        struct render_worker_data* render_worker_data;
        struct save_worker_data* save_worker_data;
    };
} WorkerInfo;

void main_thread_post(struct main_thread_work work);
// STRICT: Call from main thread only
void start_generation();
// STRICT: Call from main thread only
void stop_generation();

// Called by main thread
void push_render_queue(struct downloaded_result result);
// Called by download worker
void push_save_queue(struct render_result result);
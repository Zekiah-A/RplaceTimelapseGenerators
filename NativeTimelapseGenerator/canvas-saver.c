#include <stdlib.h>
#include "canvas-saver.h"
#include "console.h"
#include "main-thread.h"

#define LOG_HEADER "[save worker] "

void* start_save_worker(void* data)
{
    WorkerInfo* worker_data = (WorkerInfo*) data;
    worker_data->save_worker_data = (struct save_worker_data*) malloc(sizeof(struct save_worker_data));
    worker_data->save_worker_data->current_canvas_result = NULL;
    log_message(LOG_HEADER"Started save worker with thread id %d", worker_data->thread_id);

    // Enter save loop
    while (1)
    {

    }
    return NULL;
}
#include <stdlib.h>
#include "canvas-saver.h"
#include "console.h"
#include "image-generator.h"
#include "main-thread.h"

void* start_save_worker(void* data)
{
    WorkerInfo* worker_data = (WorkerInfo*) data;
    worker_data->save_worker_data = (struct save_worker_data*) malloc(sizeof(struct save_worker_data));
    worker_data->save_worker_data->current_canvas_result = NULL;
    log_message("Started save worker with thread id %d", worker_data->thread_id);

    // Enter save loop
    while (1)
    {

    }
    return NULL;
}
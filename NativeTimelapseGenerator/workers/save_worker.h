#pragma once
#include "worker_structs.h"

// Shared between all save workers
typedef struct save_worker_data
{
} SaveWorkerData;

void* start_save_worker(void* data);
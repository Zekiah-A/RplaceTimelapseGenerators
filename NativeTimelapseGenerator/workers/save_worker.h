#pragma once
#include "worker_structs.h"

// Shared between all save workers
typedef struct save_worker_data
{
} SaveWorkerShared;

// Instance / worker / per thread members
typedef struct save_worker_instance
{
} SaveWorkerInstance;

void* start_save_worker(void* data);
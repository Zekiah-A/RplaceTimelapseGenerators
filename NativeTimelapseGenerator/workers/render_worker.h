#pragma once
#include <stdint.h>

#include "worker_structs.h"

struct region_info
{
	int start_x;
	int start_y;
	int scale;
};

// Shared between all render workers
typedef struct render_worker_shared
{
} RenderWorkerShared;

// Instance / worker / per thread members
typedef struct render_worker_instance
{
} RenderWorkerInstance;

void* start_render_worker(void* data);
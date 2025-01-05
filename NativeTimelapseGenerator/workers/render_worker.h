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
typedef struct render_worker_data
{
} RenderWorkerData;
void* start_render_worker(void* data);
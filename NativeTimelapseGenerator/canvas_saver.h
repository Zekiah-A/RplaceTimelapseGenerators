#pragma once
#include "worker_structs.h"

struct save_worker_data
{
	RenderResult* current_canvas_result;
};

void* start_save_worker(void* data);
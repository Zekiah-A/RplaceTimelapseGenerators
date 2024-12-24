// Extracted code from rplace bot https://github.com/Zekiah-A/RplaceBot/blob/main/main.c
#pragma once
#include "worker_structs.h"
#include <stdint.h>

struct region_info
{
	int start_x;
	int start_y;
	int scale;
};

struct render_worker_data
{
	DownloadedResult* current_canvas_result;
	int width;
	int height;
};
void* start_render_worker(void* data);
#pragma once
#include "worker_structs.h"
#include <curl/curl.h>
#include <stdint.h>

struct download_worker_data
{
	CURL* curl_handle;
	const char* download_base_url;
	CanvasInfo current_canvas_info;
};

void* start_download_worker(void* data);
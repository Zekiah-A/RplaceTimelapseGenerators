#pragma once
#include <curl/curl.h>
#include <stdint.h>

#include "worker_structs.h"

struct download_worker_data
{
	CURL* curl_handle;
	const char* download_base_url;
	CanvasInfo current_canvas_info;
};

void* start_download_worker(void* data);
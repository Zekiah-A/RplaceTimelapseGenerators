#pragma once
#include "worker-structs.h"
#include <curl/curl.h>
#include <stdint.h>


struct download_worker_data
{
    CURL* curl_handle;
    struct canvas_info current_canvas_info;
};

void* start_download_worker(void* data);

#define DOWNLOAD_ERROR_NONE 0
#define DOWNLOAD_FAIL_FETCH 1
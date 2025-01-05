#pragma once
#include <curl/curl.h>
#include <stdint.h>

#include "worker_structs.h"

// Shared between all download workers
typedef struct download_worker_data
{
	CURL* curl_handle;
} DownloadWorkerData;

void* start_download_worker(void* data);
#pragma once
#include <curl/curl.h>
#include <stdint.h>

#include "worker_structs.h"

// Shared between all download workers
typedef struct download_worker_shared
{
	struct { uint32_t key; User* value; }* user_map;
} DownloadWorkerShared;

// Instance / worker / per thread members
typedef struct download_worker_instance
{
	CURL* curl_handle;
} DownloadWorkerInstance;

void* start_download_worker(void* data);
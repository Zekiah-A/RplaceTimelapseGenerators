#pragma once
#include <curl/curl.h>
#include <stdint.h>

#include "worker_structs.h"

typedef struct user_map_entry
{
	UserIntId key; // user_int_id
	User* value; // Pointer to the user
} UserMapEntry;

// Shared between all download workers
typedef struct download_worker_shared
{
	UserMapEntry* user_map; // stb hash map
} DownloadWorkerShared;

// Instance / worker / per thread members
typedef struct download_worker_instance
{
	CURL* curl_handle;
} DownloadWorkerInstance;

void* start_download_worker(void* data);
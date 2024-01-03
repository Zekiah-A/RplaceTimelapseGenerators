#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "canvas-downloader.h"
#include "main-thread.h"
#include "console.h"

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    char* data = (char*)userp;
    memcpy(data, contents, realsize);
    return realsize;
}

struct downloaded_result download_url(struct download_worker_data* worker_data)
{
    struct downloaded_result result = { };
 
    if (!worker_data->curl_handle)
    {
        result.error = DOWNLOAD_ERROR_NONE;
        result.error_msg = "Failed to initialize libcurl";
        return result;
    }
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_URL, worker_data->current_canvas_info.url);

    char* buffer = NULL;
    long buffer_size = 0;
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_WRITEDATA, &buffer);

    CURLcode res = curl_easy_perform(worker_data->curl_handle);

    if (res != CURLE_OK)
    {
        result.error = DOWNLOAD_FAIL_FETCH;
        result.error_msg = (char*) curl_easy_strerror(res);
    }
    else
    {
        curl_easy_getinfo(worker_data->curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &buffer_size);
        result.data = (uint8_t*) malloc(buffer_size);
        if (!result.data)
        {
            result.error = DOWNLOAD_FAIL_FETCH;
            result.error_msg = "Failed to allocate downlaod result data";
        }
        else
        {
            memcpy(result.data, buffer, buffer_size);
            result.data[buffer_size];
            result.size = buffer_size;
        }
    }
    if (buffer)
    {
        free(buffer);
    }

    return result;
}

void* start_download_worker(void* data) // struct worker_info : identical to download_worker_info 
{
    WorkerInfo* worker_info = (WorkerInfo*) data;
    worker_info->download_worker_data = (struct download_worker_data*) malloc(sizeof(struct download_worker_data));
    worker_info->download_worker_data->curl_handle = curl_easy_init();
    log_message("Started download worker with thread id %d", worker_info->thread_id);

    // Enter download loop
    while (1)
    {

    }
    return NULL;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include "canvas-downloader.h"
#include "main-thread.h"
#include "console.h"

#define LOG_HEADER "[download worker] "

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    char* data = (char*)userp;
    memcpy(data, contents, realsize);
    return realsize;
}

struct memory_fetch
{
    size_t size;
    uint8_t* memory;
};

size_t fetch_memory_callback(void* contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory_fetch* fetch = (struct memory_fetch*) userp;

    fetch->memory = realloc(fetch->memory, fetch->size + realsize);
    if (fetch->memory == NULL)
    {
        fetch->size = 0;
        fprintf(stderr, "Not enough memory to continue fetch (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(fetch->memory[fetch->size]), contents, realsize);
    fetch->size += realsize;

    return realsize;
}

struct downloaded_result download_url(struct download_worker_data* worker_data)
{
    struct downloaded_result result = { .canvas_info = worker_data->current_canvas_info };
 
    if (!worker_data->curl_handle)
    {
        result.error = DOWNLOAD_ERROR_NONE;
        result.error_msg = "Failed to initialize libcurl";
        return result;
    }

    CURLcode curl_result;
    struct memory_fetch chunk = { .memory = malloc(1), .size = 0 };
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_URL, worker_data->current_canvas_info.url);
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_WRITEFUNCTION, fetch_memory_callback);
    curl_easy_setopt(worker_data->curl_handle, CURLOPT_WRITEDATA, &chunk);

    curl_result = curl_easy_perform(worker_data->curl_handle);
    if (curl_result != CURLE_OK)
    {
        result.error = DOWNLOAD_FAIL_FETCH;
        result.error_msg = (char*) curl_easy_strerror(curl_result);
        return result;
    }

    result.data = (uint8_t*) chunk.memory;
    result.size = chunk.size;
    return result;
}

void* start_download_worker(void* data)
{
    WorkerInfo* worker_info = (WorkerInfo*) data;
    worker_info->download_worker_data = (struct download_worker_data*) malloc(sizeof(struct download_worker_data));
    worker_info->download_worker_data->curl_handle = curl_easy_init();
    log_message(LOG_HEADER"%d] Started download worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

    // Enter download loop
    while (1)
    {
        struct canvas_info info = pop_download_stack(worker_info->worker_id);
        worker_info->download_worker_data->current_canvas_info = info;
        struct downloaded_result result = download_url(worker_info->download_worker_data);
        if (result.error)
        {
            log_message(LOG_HEADER"%d] Download %s failed with error %d message %s", worker_info->worker_id, info.commit_hash, result.error, result.error_msg);
            continue;
        }
        else
        {
            push_render_stack(result);
        }
    }
    return NULL;
}
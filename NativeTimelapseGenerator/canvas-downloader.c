#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "canvas-downloader.h"

static __thread CURL* curl_handle = NULL;

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    char* data = (char*)userp;
    memcpy(data, contents, realsize);
    return realsize;
}

struct downloaded_result download_url(const char* url)
{
    struct downloaded_result result = { };

    // Initialize for thread if not already
    if (!curl_handle)
    {
        curl_handle = curl_easy_init();
        if (!curl_handle)
        {
            result.error = DOWNLOAD_ERROR_NONE;
            result.error_msg = "Failed to initialize libcurl";
            return result;
        }
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    char* buffer = NULL;
    long buffer_size = 0;
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &buffer);

    CURLcode res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK)
    {
        result.error = DOWNLOAD_FAIL_FETCH;
        result.error_msg = (char*) curl_easy_strerror(res);
    }
    else
    {
        curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &buffer_size);
        result.data = (char*) malloc(buffer_size + 1);
        if (result.data)
        {
            memcpy(result.data, buffer, buffer_size);
            result.data[buffer_size] = '\0';
        }
    }

    // Free the buffer
    if (buffer)
    {
        free(buffer);
    }

    return result;
}

// Thread exit
/*void cleanup_curl()
{
    if (curl_handle)
    {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
}*/
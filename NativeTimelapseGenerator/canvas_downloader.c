#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include "canvas_downloader.h"
#include "main_thread.h"
#include "console.h"
#include "parson/parson.h"

#define LOG_HEADER "[download worker %d] "

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t realsize = size * nmemb;
	char* data = (char*)userp;
	memcpy(data, contents, realsize);
	return realsize;
}

struct canvas_metadata {
	int width;
	int height;
	Colour* palette;
	int palette_size;
};

struct memory_fetch
{
	size_t size;
	uint8_t* memory;
};

size_t fetch_memory_callback(void* contents, size_t size, size_t nmemb, void *userp)
{
	size_t real_size = size * nmemb;
	struct memory_fetch* fetch = (struct memory_fetch*) userp;

	uint8_t* temp_memory = realloc(fetch->memory, fetch->size + real_size);
	if (temp_memory == NULL)
	{
		free(fetch->memory);
		fetch->size = 0;
		fprintf(stderr, "Not enough memory to continue fetch (realloc returned NULL)\n");
		stop_console();
		exit(EXIT_FAILURE);
	}
	fetch->memory = temp_memory;
	memcpy(&(fetch->memory[fetch->size]), contents, real_size);
	fetch->size += real_size;

	return real_size;
}

static struct memory_fetch fetch_url(const char* url, CURL* curl_handle)
{
	struct memory_fetch fetch = { .memory = malloc(1), .size = 0 };
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fetch_memory_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fetch);

	CURLcode res = curl_easy_perform(curl_handle);
	if (res != CURLE_OK)
	{
		free(fetch.memory);
		fetch.memory = NULL;
		fetch.size = 0;
	}

	return fetch;
}

struct canvas_metadata download_canvas_metadata(const char* metadata_url, CURL* curl_handle)
{
	struct canvas_metadata metadata = { .palette = NULL, .palette_size = 0 };
	struct memory_fetch metadata_response = fetch_url(metadata_url, curl_handle);

	if (metadata_response.size == 0) {
		fprintf(stderr, "Error fetching metadata from %s\n", metadata_url);
		free(metadata_response.memory);
		return metadata;
	}

	char* json_string = malloc(metadata_response.size + 1);
	if (json_string == NULL) {
		fprintf(stderr, "Memory allocation failed for JSON string\n");
		free(metadata_response.memory);
		return metadata;
	}
	memcpy(json_string, metadata_response.memory, metadata_response.size);
	json_string[metadata_response.size] = '\0';

	JSON_Value* root = json_parse_string(json_string);
	JSON_Object* root_obj = json_value_get_object(root);
	JSON_Value* palette_value = json_object_get_value(root_obj, "palette");
	JSON_Array* palette_array = json_value_get_array(palette_value);
	int palette_size = (int) json_array_get_count(palette_array);
	metadata.palette_size = palette_size;
	metadata.palette = malloc(palette_size * sizeof(Colour));
	if (metadata.palette == NULL) {
		fprintf(stderr, "Memory allocation failed for palette\n");
		free(json_string);
		json_value_free(root);
		free(metadata_response.memory);
		return metadata;
	}
	for (int i = 0; i < palette_size; i++)
	{
		uint32_t colour_int = (uint32_t) json_array_get_number(palette_array, i);
		metadata.palette[i].r = (uint8_t)(colour_int >> 24);
		metadata.palette[i].g = (uint8_t)(colour_int >> 16);
		metadata.palette[i].b = (uint8_t)(colour_int >> 8);
		metadata.palette[i].a = (uint8_t)(colour_int & 255);
	}
	JSON_Value* width_value = json_object_get_value(root_obj, "width");
	metadata.width = (int) json_value_get_number(width_value);

	JSON_Value* height_value = json_object_get_value(root_obj, "height");
	metadata.height = (int) json_value_get_number(height_value);

	free(json_string);
	json_value_free(root);
	free(metadata_response.memory);
	return metadata;
}

DownloadedResult download_url(struct download_worker_data* worker_data)
{
	DownloadedResult result = { .canvas_info = worker_data->current_canvas_info };

	if (!worker_data->curl_handle)
	{
		result.error = DOWNLOAD_ERROR_NONE;
		result.error_msg = strdup("Failed to initialize libcurl");
		return result;
	}

	const char* canvas_url_format = "%s/%s/place";
	int canvas_url_len = snprintf(NULL, 0, canvas_url_format, worker_data->download_base_url,
		worker_data->current_canvas_info.commit_hash);
	char* canvas_url = malloc(canvas_url_len + 1);
	if (canvas_url == NULL) {
		result.error = DOWNLOAD_ERROR_NONE;
		result.error_msg = strdup("Memory allocation failed for canvas URL");
		return result;
	}
	snprintf(canvas_url, canvas_url_len + 1, canvas_url_format, worker_data->download_base_url,
		worker_data->current_canvas_info.commit_hash);

	struct memory_fetch data = fetch_url(canvas_url, worker_data->curl_handle);
	if (data.size == 0)
	{
		result.error = DOWNLOAD_FAIL_FETCH;
		result.error_msg = strdup("Failed to fetch canvas data");
		free(canvas_url);
		return result;
	}

	// Download and parse metadata
	const char* metadata_url_format = "%s/%s/metadata.json";
	int metadata_url_len = snprintf(NULL, 0, metadata_url_format, worker_data->download_base_url,
		worker_data->current_canvas_info.commit_hash);
	char* metadata_url = malloc(metadata_url_len + 1);
	if (metadata_url == NULL) {
		result.error = DOWNLOAD_ERROR_NONE;
		result.error_msg = strdup("Memory allocation failed for metadata URL");
		free(canvas_url);
		free(data.memory);
		return result;
	}
	snprintf(metadata_url, metadata_url_len + 1, metadata_url_format, worker_data->download_base_url,
		worker_data->current_canvas_info.commit_hash);

	struct canvas_metadata metadata = download_canvas_metadata(metadata_url, worker_data->curl_handle);
	if (metadata.palette == NULL) {
		result.error = DOWNLOAD_FAIL_METADATA;
		result.error_msg = strdup("Failed to download or parse metadata");
		free(canvas_url);
		free(metadata_url);
		free(data.memory);
		return result;
	}

	result.canvas = (uint8_t*) data.memory;
	result.canvas_size = data.size;
	result.width = metadata.width;
	result.height = metadata.height;
	result.palette_size = metadata.palette_size;
	result.palette = metadata.palette;

	free(canvas_url);
	free(metadata_url);
	return result;
}

void* start_download_worker(void* data)
{
	WorkerInfo* worker_info = (WorkerInfo*) data;
	worker_info->download_worker_data = (struct download_worker_data*) malloc(sizeof(struct download_worker_data));
	worker_info->download_worker_data->curl_handle = curl_easy_init();
	worker_info->download_worker_data->download_base_url = get_download_base_url();
	log_message(LOG_HEADER"Started download worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter download loop
	while (true) {
		CanvasInfo info = pop_download_stack(worker_info->worker_id);
		worker_info->download_worker_data->current_canvas_info = info;
		DownloadedResult result = download_url(worker_info->download_worker_data);
		if (result.error) {
			log_message(LOG_HEADER"Download %s failed with error %d message %s", worker_info->worker_id, info.commit_hash, result.error, result.error_msg);
			continue;
		}
		else {
			push_render_stack(result);
		}
	}
	return NULL;
}
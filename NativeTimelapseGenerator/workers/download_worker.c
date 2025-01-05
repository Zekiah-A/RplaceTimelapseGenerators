#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>

#include "download_worker.h"
#include "worker_enums.h"

#include "../console.h"
#include "../memory_utils.h"
#include "../main_thread.h"

#include "../lib/parson/parson.h"
#include "../lib/stb/stb_ds.h"

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

struct top_placers {
	Placer* placers;
	size_t size;
};

struct memory_fetch {
	size_t size;
	uint8_t* memory;
};

const Config* _config;
DownloadWorkerData* _worker_data;

size_t fetch_memory_callback(void* contents, size_t size, size_t nmemb, void *userp)
{
	size_t real_size = size * nmemb;
	struct memory_fetch* fetch = (struct memory_fetch*) userp;

	uint8_t* temp_memory = realloc(fetch->memory, fetch->size + real_size);
	if (temp_memory == NULL) {
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
	if (res != CURLE_OK) {
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

	AUTOFREE char* json_string = malloc(metadata_response.size + 1);
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
		json_value_free(root);
		free(metadata_response.memory);
		return metadata;
	}
	for (int i = 0; i < palette_size; i++) {
		uint32_t colour_int = (uint32_t) json_array_get_number(palette_array, i);
		metadata.palette[i].r = (uint8_t)((colour_int >> 24) & 0xFF);
		metadata.palette[i].g = (uint8_t)((colour_int >> 16) & 0xFF);
		metadata.palette[i].b = (uint8_t)((colour_int >> 8) & 0xFF);
		metadata.palette[i].a = (uint8_t)(colour_int & 0xFF);
	}
	JSON_Value* width_value = json_object_get_value(root_obj, "width");
	metadata.width = (int) json_value_get_number(width_value);

	JSON_Value* height_value = json_object_get_value(root_obj, "height");
	metadata.height = (int) json_value_get_number(height_value);

	json_value_free(root);
	free(metadata_response.memory);
	return metadata;
}

User* get_user(int int_id)
{
	// TODO: Pull from global download worker data hash map caches instead of requesting if already exists

	const char* user_url_format = "%s/users/%d";
	int user_url_len = snprintf(NULL, 0, user_url_format, _config->game_server_base_url, int_id);
	AUTOFREE char* user_url = malloc(user_url_len + 1);
	snprintf(user_url, user_url_len + 1, user_url_format, _config->game_server_base_url, int_id);

	struct memory_fetch user_response = fetch_url(user_url, _worker_data->curl_handle);
	if (user_response.size == 0) {
		return NULL;
	}

	AUTOFREE char* json_string = malloc(user_response.size + 1);
	memcpy(json_string, user_response.memory, user_response.size);
	json_string[user_response.size] = '\0';

	User* user = malloc(sizeof(User));
	JSON_Value* root = json_parse_string(json_string);
	JSON_Object* root_obj = json_value_get_object(root);
	JSON_Value* chat_name_value = json_object_get_value(root_obj, "chatName");
	user->chat_name = json_value_get_string(chat_name_value);
	JSON_Value* last_joined_value = json_object_get_value(root_obj, "lastJoined");
	user->last_joined = (time_t) json_value_get_number(last_joined_value);
	JSON_Value* pixels_placed_value = json_object_get_value(root_obj, "pixelsPlaced");
	user->pixels_placed = json_value_get_number(pixels_placed_value);
	JSON_Value* play_time_seconds_value = json_object_get_value(root_obj, "playTimeSeconds");
	user->play_time_seconds = json_value_get_number(play_time_seconds_value);
	return user;
}


struct top_placers get_top_placers(uint32_t* placers, size_t placers_size, size_t max_count)
{
	struct { uint32_t key; uint32_t value; }* placer_counts = NULL;
	
	for (uint32_t i = 0; i < placers_size; i++) {
		int user_int_id = placers[i];
		uint32_t current_placed = hmget(placer_counts, i);
		hmput(placer_counts, user_int_id, current_placed + 1);
	}

	Placer* top_placers = calloc(max_count, sizeof(User));

	// Iterate placer counts hashmap (once)
	for (int i = 0; i < hmlen(placer_counts); i++) {
		uint32_t user_int_id = placer_counts[i].key;
		uint32_t placed = placer_counts[i].value;

		// Compare against top placers and reorder if necessary
		for (size_t j = 0; j < max_count; j++) {
			if (placed > top_placers[i].pixels_placed) {
				// We only request the user if they are actually going to end up on the leaderboard
				User* user = get_user(user_int_id);
				if (user == NULL) {
					break;
				}
				
				// Move existing top placers down
				memcpy(&top_placers[i - 1], &top_placers[i], (max_count - i - 1) * sizeof(Placer));
				
				// Add new top placer to top placers list
				Placer new_top_placer = { .int_id =  user_int_id, .chat_name = user->chat_name, .pixels_placed = placed };
				top_placers[i] = new_top_placer;
				break;
			}
		}
	}

	struct top_placers result = { .placers = top_placers, .size = max_count };
	return result;
}

DownloadResult download(CanvasInfo* canvas_info)
{
	DownloadResult result = { .canvas_info = canvas_info };

	const char* canvas_url_format = "%s/%s/place";
	int canvas_url_len = snprintf(NULL, 0, canvas_url_format, _config->download_base_url,
		canvas_info->commit_hash);
	AUTOFREE char* canvas_url = malloc(canvas_url_len + 1);
	if (canvas_url == NULL) {
		result.error = DOWNLOAD_ERROR_NONE;
		result.error_msg = strdup("Memory allocation failed for canvas URL");
		return result;
	}
	snprintf(canvas_url, canvas_url_len + 1, canvas_url_format, _config->download_base_url,
		canvas_info->commit_hash);

	struct memory_fetch canvas_data = fetch_url(canvas_url, _worker_data->curl_handle);
	if (canvas_data.size == 0) {
		result.error = DOWNLOAD_FAIL_FETCH;
		result.error_msg = strdup("Failed to fetch canvas data");
		return result;
	}

	// Download and parse metadata
	const char* metadata_url_format = "%s/%s/metadata.json";
	int metadata_url_len = snprintf(NULL, 0, metadata_url_format, _config->download_base_url,
		canvas_info->commit_hash);
	AUTOFREE char* metadata_url = malloc(metadata_url_len + 1);
	if (metadata_url == NULL) {
		result.error = DOWNLOAD_ERROR_NONE;
		result.error_msg = strdup("Memory allocation failed for metadata URL");
		free(canvas_data.memory);
		return result;
	}
	snprintf(metadata_url, metadata_url_len + 1, metadata_url_format, _config->download_base_url,
		canvas_info->commit_hash);

	struct canvas_metadata metadata = download_canvas_metadata(metadata_url, _worker_data->curl_handle);
	if (metadata.palette == NULL) {
		result.error = DOWNLOAD_FAIL_METADATA;
		result.error_msg = strdup("Failed to download or parse metadata");
		free(canvas_data.memory);
		return result;
	}

	// Download placers
	const char* placers_url_format = "%s/%s/placers";
	int placers_url_len = snprintf(NULL, 0, placers_url_format, _config->download_base_url,
		canvas_info->commit_hash);
	AUTOFREE char* placers_url = malloc(placers_url_len + 1);
	snprintf(placers_url, placers_url_len + 1, placers_url_format, _config->download_base_url,
		canvas_info->commit_hash);
	if (metadata_url == NULL) {
		result.error = DOWNLOAD_FAIL_METADATA;
		result.error_msg = strdup("Memory allocation failed for placers URL");
		free(canvas_data.memory);
		return result;
	}

	struct memory_fetch placers_data = fetch_url(placers_url, _worker_data->curl_handle);
	if (placers_data.size == 0) {
		result.error = DOWNLOAD_FAIL_FETCH;
		result.error_msg = strdup("Failed to fetch placers data");
		return result;
	}

	struct top_placers top_placers = get_top_placers((uint32_t*) placers_data.memory, placers_data.size, 
		_config->max_top_placers);

	result.canvas = (uint8_t*) canvas_data.memory;
	result.canvas_size = canvas_data.size;
	result.width = metadata.width;
	result.height = metadata.height;
	result.palette_size = metadata.palette_size;
	result.palette = metadata.palette;
	result.placers_size = placers_data.size / 4;
	result.placers = (uint32_t*) placers_data.memory;
	result.top_placers_size = top_placers.size;
	result.top_placers = top_placers.placers;
	return result;
}

void* start_download_worker(void* data)
{
	// Initialise worker / thread globals
	WorkerInfo* worker_info = (WorkerInfo*) data;
	_config = worker_info->config;
	_worker_data = worker_info->download_worker_data;

	log_message(LOG_HEADER"Started download worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter download loop
	while (true) {
		CanvasInfo info = pop_download_stack(worker_info->worker_id);
		DownloadResult result = download(worker_info->current_canvas_info);
		if (result.error) {
			log_message(LOG_HEADER"Download %s failed with error %d message %s", worker_info->worker_id, info.commit_hash, result.error, result.error_msg);
			continue;
		}
		push_render_stack(result);

	}
	return NULL;
}
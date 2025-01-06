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

#define STB_DS_IMPLEMENTATION
#include "../lib/stb/stb_ds.h"
#include "../lib/parson/parson.h"
#include "worker_structs.h"

#define LOG_HEADER "[download worker %d] "

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

struct fetch_result {
	size_t size;
	uint8_t* memory;

	CURLcode error;
	const char* error_msg;
};

size_t fetch_memory_callback(void* contents, size_t size, size_t nmemb, void* userp) {
	size_t real_size;
	struct fetch_result* fetch = (struct fetch_result*)userp;

	// Check for size overflow
	if (size > 0 && nmemb > SIZE_MAX / size) {
		stop_console();
		fprintf(stderr, "Size overflow in fetch_memory_callback\n");
		exit(EXIT_FAILURE);
	}
	real_size = size * nmemb;

	// Reallocate memory
	uint8_t* temp_memory = realloc(fetch->memory, fetch->size + real_size);
	if (temp_memory == NULL) {
		free(fetch->memory);
		stop_console();
		fprintf(stderr, "Not enough memory to continue fetch (realloc returned NULL)\n");
		exit(EXIT_FAILURE);
	}
	fetch->memory = temp_memory;

	// Copy new data to the allocated memory
	memcpy(&(fetch->memory[fetch->size]), contents, real_size);
	fetch->size += real_size;
	return real_size;
}

static struct fetch_result fetch_url(const char* url, CURL* curl_handle)
{
	struct fetch_result fetch = {
		.memory = NULL,
		.size = 0
	};
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fetch_memory_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fetch);

	fetch.error = curl_easy_perform(curl_handle);
	if (fetch.error != CURLE_OK) {
		free(fetch.memory);
		fetch.error_msg = curl_easy_strerror(fetch.error);
		fetch.memory = NULL;
		fetch.size = 0;
	}

	return fetch;
}

struct canvas_metadata download_canvas_metadata(const char* metadata_url, CURL* curl_handle)
{
	struct canvas_metadata metadata = { .palette = NULL, .palette_size = 0 };
	struct fetch_result metadata_response = fetch_url(metadata_url, curl_handle);

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

User* get_user(const WorkerInfo* worker_info, int int_id)
{
	const Config* config = worker_info->config;
	DownloadWorkerInstance* instance = worker_info->download_worker_instance;

	// TODO: Pull from global download worker data hash map caches instead of requesting if already exists

	AUTOFREE char* user_url = NULL;
	asprintf(&user_url, "%s/users/%d", config->game_server_base_url, int_id);

	struct fetch_result user_response = fetch_url(user_url, instance->curl_handle);
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


struct top_placers get_top_placers(const WorkerInfo* worker_info, uint32_t* placers, size_t placers_size, size_t max_count)
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
				User* user = get_user(worker_info, user_int_id);
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

DownloadResult download(const WorkerInfo* worker_info, CanvasInfo canvas_info)
{
	const Config* config = worker_info->config;
	DownloadWorkerInstance* instance = worker_info->download_worker_instance;
	DownloadResult result = {
		// Error handling
		.download_error = DOWNLOAD_ERROR_NONE,
		.error_msg = NULL,

		// Previous structs
		.canvas_info = canvas_info
	};

	AUTOFREE char* canvas_url = NULL;
	asprintf(&canvas_url, "%s/%s/place", config->download_base_url, canvas_info.commit_hash);

	struct fetch_result canvas_data = fetch_url(canvas_url, instance->curl_handle);
	if (canvas_data.error != CURLE_OK) {
		result.download_error = DOWNLOAD_FAIL_FETCH;
		asprintf(&result.error_msg, "Failed to fetch canvas data: %s", canvas_data.error_msg);
		return result;
	}

	// Download and parse metadata
	AUTOFREE char* metadata_url = NULL;
	asprintf(&metadata_url, "%s/%s/metadata.json", config->download_base_url, canvas_info.commit_hash);

	struct canvas_metadata metadata = download_canvas_metadata(metadata_url, instance->curl_handle);
	if (metadata.palette == NULL) {
		result.download_error = DOWNLOAD_FAIL_METADATA;
		result.error_msg = strdup("Failed to download or parse metadata");
		free(canvas_data.memory);
		return result;
	}

	// Download placers
	AUTOFREE char* placers_url = NULL;
	asprintf(&placers_url, "%s/%s/placers", config->download_base_url, canvas_info.commit_hash);

	struct fetch_result placers_data = fetch_url(placers_url, instance->curl_handle);
	if (placers_data.error != CURLE_OK) {
		result.download_error = DOWNLOAD_FAIL_FETCH;
		asprintf(&result.error_msg, "Failed to fetch placers data: %s", placers_data.error_msg);
		free(canvas_data.memory);
		return result;
	}

	struct top_placers top_placers = get_top_placers(worker_info, (uint32_t*) placers_data.memory,
		placers_data.size, config->max_top_placers);

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
	const WorkerInfo* worker_info = (const WorkerInfo*) data;

	// Initialise instance members
	CURL* curl_handle = curl_easy_init();
	worker_info->download_worker_instance->curl_handle = curl_handle;

	log_message(LOG_HEADER"Started download worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter download loop
	while (true) {
		CanvasInfo canvas_info = pop_download_stack(worker_info->worker_id);

		DownloadResult result = download(worker_info, canvas_info);
		if (result.download_error != DOWNLOAD_ERROR_NONE) {
			log_message(LOG_HEADER"Download %s failed with error %d message %s", worker_info->worker_id, canvas_info.commit_hash, result.download_error, result.error_msg);
			continue;
		}
		push_render_stack(result);
	}
	return NULL;
}
#include <avcall.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include <avcall.h>

#include "download_worker.h"
#include "worker_enums.h"

#include "../console.h"
#include "../memory_utils.h"
#include "../main_thread.h"
#include "../database.h"

#include "../lib/stb/stb_ds.h"
#include "../lib/parson/parson.h"
#include "worker_structs.h"

#define LOG_HEADER "[download worker %d] "

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

Colour colour_hash(const char* text)
{
	uint32_t hash = 0;
	while (*text) {
		hash = (hash * 31 + (unsigned char)(*text)) & 0xFFFFFFFF;
		text++;
	}
	return (Colour) {
		.r = (hash >> 24) & 0xFF,
		.g = (hash >> 16) & 0xFF,
		.b = (hash >> 8) & 0xFF,
		.a = hash & 0xFF
	};
}

size_t fetch_memory_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t real_size;
	struct fetch_result* fetch = (struct fetch_result*)userp;

	// Check for size overflow
	if (size > 0 && nmemb > SIZE_MAX / size) {
		stop_console();
		log_message(LOG_ERROR, "Size overflow in fetch_memory_callback\n");
		exit(EXIT_FAILURE);
	}
	real_size = size * nmemb;

	// Reallocate memory
	uint8_t* temp_memory = realloc(fetch->memory, fetch->size + real_size);
	fetch->memory = temp_memory;

	// Copy new data to the allocated memory
	memcpy(&(fetch->memory[fetch->size]), contents, real_size);
	fetch->size += real_size;
	return real_size;
}

uint32_t swap32(uint32_t value) {
	return ((value >> 24) & 0xff) |
			((value >> 8) & 0xff00) |
			((value << 8) & 0xff0000) |
			((value << 24) & 0xff000000);
}

static struct fetch_result fetch_url(const char* url, CURL* curl_handle)
{
	struct fetch_result fetch = {
		.error = CURLE_OK,
		.error_msg = NULL,

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
	CanvasMetadata metadata = { .palette = NULL, .palette_size = 0 };
	struct fetch_result metadata_response = fetch_url(metadata_url, curl_handle);

	if (metadata_response.size == 0) {
		log_message(LOG_ERROR, "Error fetching metadata from %s\n", metadata_url);
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
		log_message(LOG_ERROR, "Memory allocation failed for palette\n");
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

User* get_user(const WorkerInfo* worker_info, UserIntId int_id)
{
	const Config* config = worker_info->config;
	DownloadWorkerShared* shared = worker_info->download_worker_shared;
	DownloadWorkerInstance* instance = worker_info->download_worker_instance;

	// Check cache first
	if (shared->user_map != NULL) {
		User* cached_user = hmget(shared->user_map, int_id);
		if (cached_user != NULL) {
			return cached_user;
		}
	}

	AUTOFREE char* user_url = NULL;
	if (asprintf(&user_url, "%s/users/%d", config->game_server_base_url, int_id) == -1) {
		return NULL;
	}

	struct fetch_result user_response = fetch_url(user_url, instance->curl_handle);
	if (user_response.size == 0) {
		return NULL;
	}

	AUTOFREE char* json_string = malloc(user_response.size + 1);
	memcpy(json_string, user_response.memory, user_response.size);
	json_string[user_response.size] = '\0';

	JSON_Value* root = json_parse_string(json_string);
	if (!root) {
		return NULL;
	}
	JSON_Object* root_obj = json_value_get_object(root);
	if (!root_obj) {
		json_value_free(root);
		return NULL;
	}
	User* user = malloc(sizeof(User));
	if (!user) {
		json_value_free(root);
		return NULL;
	}
	const char* chat_name = json_object_get_string(root_obj, "chatName");
	if (!chat_name) {
		user->chat_name = NULL;
	}
	else {
		user->chat_name = strdup(chat_name);
	}
	user->last_joined = (time_t)json_object_get_number(root_obj, "lastJoined");
	user->pixels_placed = (uint32_t) json_object_get_number(root_obj, "pixelsPlaced");
	user->play_time_seconds = (uint32_t) json_object_get_number(root_obj, "playTimeSeconds");
	json_value_free(root);

	// Cache user in global download worker user map
	hmput(shared->user_map, int_id, user);
	return user;
}

typedef struct placer_counts_entry {
	UserIntId key; // user_int_id
	uint32_t value; // pixels_placed
} PlacerCountsEntry;

struct top_placers get_top_placers(const WorkerInfo* worker_info, UserIntId* placers, size_t placers_size, size_t max_count)
{
    if (!placers || placers_size == 0 || max_count == 0) {
		struct top_placers result = { 0 };
        return result;
    }

	// Count placer occurrances
	PlacerCountsEntry* placer_counts_map = NULL;
	for (uint32_t i = 0; i < placers_size; i++) {
		UserIntId user_int_id = placers[i];
		PlacerCountsEntry* entry = hmgetp_null(placer_counts_map, user_int_id);
		if (!entry) {
			hmput(placer_counts_map, user_int_id, 1);
		}
		else {
			uint32_t current_placed = entry->value;
			hmput(placer_counts_map, user_int_id, current_placed + 1);

		}
	}

	Placer* top_placers = calloc(max_count, sizeof(User));

	// Iterate placer counts hashmap (once)
	size_t current_count = 0;
	for (int i = 0; i < hmlen(placer_counts_map); i++) {
		UserIntId user_int_id = placer_counts_map[i].key;
		uint32_t placed = placer_counts_map[i].value;

		// Find insertion position
        size_t insert_pos = current_count;
        while (insert_pos > 0 && placed > top_placers[insert_pos-1].pixels_placed) {
            insert_pos--;
        }

        if (insert_pos < max_count) {
            User* user = get_user(worker_info, user_int_id);
            if (!user) {
				log_message(LOG_ERROR, LOG_HEADER"Failed to get user with int id %lu when calculating top placers",
					worker_info->worker_id, user_int_id);
				continue;
			}

            // Make room if array isn't full yet
            if (current_count < max_count) {
                current_count++;
            }

            // Shift elements up to make space
            if (insert_pos < current_count - 1) {
                memmove(&top_placers[insert_pos+1], 
                       &top_placers[insert_pos],
                       (current_count - insert_pos - 1) * sizeof(Placer));
            }

            // Insert new placer
            top_placers[insert_pos] = (Placer){
                .int_id = user_int_id,
                .chat_name = user->chat_name,
                .pixels_placed = placed,
                .colour = colour_hash(user->chat_name)
            };
        }
	}

	// Shrink allocation if necessary
	if (current_count < max_count) {
        Placer* tmp = realloc(top_placers, current_count * sizeof(Placer));
        if (tmp) {
			top_placers = tmp;
		} 
    }

	hmfree(placer_counts_map);
	struct top_placers result = { .placers = top_placers, .size = max_count };
	return result;
}

DownloadResult* download(const WorkerInfo* worker_info, DownloadJob job)
{
	const Config* config = worker_info->config;
	DownloadWorkerInstance* instance = worker_info->download_worker_instance;

	// Download and parse metadata
	AUTOFREE char* metadata_url = NULL;
	asprintf(&metadata_url, "%s/%s/metadata.json", config->download_base_url, job.commit_hash);

	CanvasMetadata metadata = download_canvas_metadata(metadata_url, instance->curl_handle);
	if (metadata.palette == NULL) {
		DownloadResult* results = NULL;
		DownloadResult result = (DownloadResult) { .download_error = DOWNLOAD_FAIL_METADATA, .error_msg = strdup("Failed to download or parse metadata") };
		arrput(results, result);
		return results;
	}
	// Add canvas metadata to DB
	av_alist metadata_save_alist;
	av_start_void(metadata_save_alist, &add_canvas_metadata_to_db);
	av_struct(metadata_save_alist, CanvasMetadata, metadata); 
	database_thread_post(metadata_save_alist);

	switch (job.type) {
		case DOWNLOAD_CANVAS: {
			AUTOFREE char* canvas_url = NULL;
			asprintf(&canvas_url, "%s/%s/place", config->download_base_url, job.commit_hash);

			struct fetch_result canvas_data = fetch_url(canvas_url, instance->curl_handle);
			if (canvas_data.error != CURLE_OK) {
				char* error_msg = NULL;
				asprintf(&error_msg, "Failed to fetch canvas data: %s", canvas_data.error_msg);
				DownloadResult* results = NULL;
				DownloadResult result = (DownloadResult) { .download_error = DOWNLOAD_FAIL_FETCH, .error_msg = error_msg };
				arrput(results, result);
				return results;
			}

			DownloadResult* results = NULL;
			DownloadResult canvas_save_result = {
				// Inherited from WorkerResult
				.download_error = DOWNLOAD_ERROR_NONE,
				.error_msg = NULL,
				// Members
				.job_type = JOB_TYPE_SAVE,
				.save_job = {
					// Inherited from WorkerJob
					.commit_id = job.commit_id,
					.commit_hash = job.commit_hash,
					.date = job.date,
					// Members
					.type = SAVE_CANVAS_DOWNLOAD,
					.data = canvas_data.memory,
					.size = canvas_data.size
				}
			};
			arrput(results, canvas_save_result);

			DownloadResult canvas_render_result = {
				// Inherited from WorkerResult
				.download_error = DOWNLOAD_ERROR_NONE,
				.error_msg = NULL,
				// Members
				.job_type = JOB_TYPE_RENDER,
				.render_job = {
					// Inherited from WorkerJob
					.commit_id = job.commit_id,
					.commit_hash = job.commit_hash,
					.date = job.date,
					// Members
					.type = RENDER_CANVAS,
					.canvas = {
						.width = metadata.width,
						.height = metadata.height,
						.palette_size = metadata.palette_size,
						.palette = metadata.palette,
						.size = canvas_data.size,
						.data = canvas_data.memory
					}
				}
			};
			arrput(results, canvas_render_result);
			return results;
		}
		case DOWNLOAD_PLACERS: {
			AUTOFREE char* placers_url = NULL;
			asprintf(&placers_url, "%s/%s/placers", config->download_base_url, job.commit_hash);
			struct fetch_result placers_data = fetch_url(placers_url, instance->curl_handle);
			if (placers_data.error != CURLE_OK) {
				char* error_msg = NULL;
				asprintf(&error_msg, "Failed to fetch placers data: %s", placers_data.error_msg);
				DownloadResult* results = NULL;
				DownloadResult result = (DownloadResult) { .download_error = DOWNLOAD_FAIL_FETCH, .error_msg = error_msg };
				arrput(results, result);
				return results;
			}

			// Placers is big endian, we assume we are little endian, so a swap must be performed
			uint32_t* placers_data_u32 = (uint32_t*)(void*)placers_data.memory;
			UserIntId* placers = (UserIntId*) malloc(placers_data.size);
			size_t placers_u32_size = placers_data.size / sizeof(UserIntId);
			for (size_t i = 0; i < placers_u32_size; i++) {
				placers[i] = swap32(placers_data_u32[i]);
			}

			// Produce download result
			DownloadResult* results = NULL;
			struct top_placers top_placers = get_top_placers(worker_info, placers,
				placers_u32_size, config->max_top_placers);

			DownloadResult placers_save_result = {
				// Inherited from WorkerResult
				.download_error = DOWNLOAD_ERROR_NONE,
				.error_msg = NULL,
				// Members
				.job_type = JOB_TYPE_SAVE,
				.save_job = {
					// Inherited from WorkerJob
					.commit_id = job.commit_id,
					.commit_hash = job.commit_hash,
					.date = job.date,
					// Members
					.type = SAVE_PLACERS_DOWNLOAD,
					.data = placers_data.memory,
					.size = placers_data.size
				}
			};
			arrput(results, placers_save_result);

			DownloadResult top_placers_result = {
				// Inherited from WorkerResult
				.download_error = DOWNLOAD_ERROR_NONE,
				.error_msg = NULL,
				// Members
				.job_type = JOB_TYPE_RENDER,
				.render_job = {
					// Inherited from WorkerJob
					.commit_id = job.commit_id,
					.commit_hash = job.commit_hash,
					.date = job.date,
					// Members
					.type = RENDER_TOP_PLACERS,
					.top_placers = {
						.top_placers = top_placers.placers,
						.top_placers_size = top_placers.size
					}
				}
			};
			arrput(results, top_placers_result);

			DownloadResult canvas_control_result = {
				// Inherited from WorkerResult
				.download_error = DOWNLOAD_ERROR_NONE,
				.error_msg = NULL,
				// Members
				.job_type = JOB_TYPE_RENDER,
				.render_job = {
					// Inherited from WorkerJob
					.commit_id = job.commit_id,
					.commit_hash = job.commit_hash,
					.date = job.date,
					// Members
					.type = RENDER_CANVAS_CONTROL,
					.canvas_control = {
						// Inherited from RenderJobTopPlacers
						.top_placers = top_placers.placers,
						.top_placers_size = top_placers.size,
						// Members
						.width = metadata.width,
						.height = metadata.height,
						.placers = placers,
						.placers_size = placers_u32_size
					}
				}
			};
			arrput(results, canvas_control_result);
			return results;
		}
		default: {
			DownloadResult* results = NULL;
			DownloadResult result = (DownloadResult) { .download_error = DOWNLOAD_FAIL_TYPE, .error_msg = strdup("Invalid download job type") };
			arrput(results, result);
			return results;
		}
	}
}

void* start_download_worker(void* data)
{
	// Initialise worker / thread globals
	const WorkerInfo* worker_info = (const WorkerInfo*) data;

	// Initialise instance members
	CURL* curl_handle = curl_easy_init();
	worker_info->download_worker_instance->curl_handle = curl_handle;

	log_message(LOG_INFO, LOG_HEADER"Started download worker with thread id %d",
		worker_info->worker_id, worker_info->thread_id);

	// Enter download loop
	while (true) {
		DownloadJob job = pop_download_stack(worker_info->worker_id);

		DownloadResult* results = download(worker_info, job);
		for (int i = 0; i < arrlen(results); i++) {
			DownloadResult result = results[i];
			if (result.download_error != DOWNLOAD_ERROR_NONE) {
				log_message(LOG_ERROR, LOG_HEADER"Download %s failed with error %d message %s",
					worker_info->worker_id, job.commit_hash, result.download_error, result.error_msg);
				free(result.error_msg);
				continue;
			}

			if (result.job_type == JOB_TYPE_RENDER) {
				push_render_stack(result.render_job);
			}
			else if (result.job_type == JOB_TYPE_SAVE) {
				push_save_stack(result.save_job);
			}
			else {
				log_message(LOG_ERROR, LOG_HEADER"Invalid job type %d", worker_info->worker_id, result.job_type);
			}
		}
		arrfree(results);
	}
	return NULL;
}

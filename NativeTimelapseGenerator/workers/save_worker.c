#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "worker_structs.h"
#include "save_worker.h"

#include "../console.h"
#include "../main_thread.h"
#include "../memory_utils.h"

#define LOG_HEADER "[save worker %d] "

const Config* _config;
SaveWorkerData* _worker_data;

SaveResult save(RenderResult render_result)
{
	SaveResult result = {
		.canvas_info = render_result.canvas_info,
		.download_result =  render_result.download_result,
		.render_result = render_result,
		.error_msg = NULL
	};

	char timestamp[64];
	struct tm timeinfo;
	if (gmtime_r(&render_result.canvas_info.date, &timeinfo) == NULL) {
		result.error_msg = strdup("Failed to convert time to GMT");
		return result;
	}
	if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo) == 0) {
		result.error_msg = strdup("Failed to format timestamp");
		return result;
	}
	AUTOFREE char* save_path = malloc(8 + strlen(timestamp) + 4 + 1);
	strcpy(save_path, "backups/");
	strcat(save_path, timestamp);
	strcat(save_path, ".png");

	FILE* image_file_stream = fopen(save_path, "wb");
	if (!image_file_stream) {
		result.error_msg = strdup("Couldn't open file for writing");
		return result;
	}
	size_t written = fwrite(render_result.image, sizeof(uint8_t), render_result.image_size, image_file_stream);
	if (written != render_result.image_size) {
		result.error_msg = strdup("Couldn't write the complete image");
		return result;
	}
	fclose(image_file_stream);

	// Save date image
	AUTOFREE char* date_save_path = malloc(6 + strlen(timestamp) + 4 + 1);
	strcpy(date_save_path, "dates/");
	strcat(date_save_path, timestamp);
	strcat(date_save_path, ".png");

	// TODO: Save placers image


	FILE* date_image_file_stream = fopen(date_save_path, "wb");
	if (!date_image_file_stream) {
		result.error_msg = strdup("Couldn't open date image file for writing");
		return result;
	}
	written = fwrite(render_result.date_image, sizeof(uint8_t), render_result.date_image_size, date_image_file_stream);
	if (written != render_result.date_image_size) {
		result.error_msg = strdup("Couldn't write the complete date image");
		return result;
	}
	fclose(date_image_file_stream);

	return result;
}

void* start_save_worker(void* data)
{
	WorkerInfo* worker_info = (WorkerInfo*) data;
	_config = worker_info->config;
	_worker_data = worker_info->save_worker_data;

	log_message(LOG_HEADER"Started save worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter save loop
	while (true) {
		RenderResult render_result = pop_save_stack(worker_info->worker_id);

		SaveResult result = save(render_result);
		if (result.error_msg) {
			log_message(LOG_HEADER"Save worker %d failed with error: %s", worker_info->worker_id, worker_info->worker_id, result.error_msg);
			continue;
		}

		push_completed(result);
	}
	return NULL;
}
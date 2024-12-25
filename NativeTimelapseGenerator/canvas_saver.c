#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_saver.h"
#include <unistd.h> // for access and F_OK
#include "console.h"
#include "main_thread.h"
#include "worker_structs.h"

#define LOG_HEADER "[save worker %d] "

void* start_save_worker(void* data)
{
	WorkerInfo* worker_info = (WorkerInfo*) data;
	worker_info->save_worker_data = (struct save_worker_data*) malloc(sizeof(struct save_worker_data));
	if (!worker_info->save_worker_data) {
		log_message(LOG_HEADER"Failed to allocate memory for save_worker_data", worker_info->worker_id);
		return NULL;
	}
	worker_info->save_worker_data->current_canvas_result = NULL;
	log_message(LOG_HEADER"Started save worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter save loop
	while (true) {
		RenderResult result = pop_save_stack(worker_info->worker_id);
		CanvasInfo info = result.canvas_info;

		char timestamp[64];
		struct tm timeinfo;
		if (gmtime_r(&info.date, &timeinfo) == NULL) {
			log_message(LOG_HEADER"Failed to convert time to GMT", worker_info->worker_id);
			continue;
		}
		if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo) == 0) {
			log_message(LOG_HEADER"Failed to format timestamp", worker_info->worker_id);
			continue;
		}
		char* save_path = malloc(8 + strlen(timestamp) + 4 + 1);
		if (!save_path) {
			log_message(LOG_HEADER"Failed to allocate memory for save_path", worker_info->worker_id);
			continue;
		}
		strcpy(save_path, "backups/");
		strcat(save_path, timestamp);
		strcat(save_path, ".png");

		// If we don't already have this backup rendered, add it to stack
		if (access(save_path, F_OK) == 0) {
			free(save_path);
			continue;
		}

		FILE* image_file_stream = fopen(save_path, "wb");
		if (!image_file_stream) {
			log_message(LOG_HEADER"Save worker %d failed with error couldn't open file for writing", worker_info->worker_id, worker_info->worker_id, info.commit_hash);
			free(save_path);
			continue;
		}
		size_t written = fwrite(result.image, sizeof(uint8_t), result.image_size, image_file_stream);
		if (written != result.image_size) {
			log_message(LOG_HEADER"Save worker %d failed with error couldn't write the complete image", worker_info->worker_id, worker_info->worker_id, info.commit_hash);
		}
		fclose(image_file_stream);
		free(save_path);

		// Save date image
		char* date_save_path = malloc(6 + strlen(timestamp) + 4 + 1);
		if (!date_save_path) {
			log_message(LOG_HEADER"Failed to allocate memory for date_save_path", worker_info->worker_id);
			continue;
		}
		strcpy(date_save_path, "dates/");
		strcat(date_save_path, timestamp);
		strcat(date_save_path, ".png");

		FILE* date_image_file_stream = fopen(date_save_path, "wb");
		if (!date_image_file_stream) {
			log_message(LOG_HEADER"Save worker %d failed with error couldn't open date image file for writing", worker_info->worker_id, worker_info->worker_id, info.commit_hash);
			free(date_save_path);
			continue;
		}
		written = fwrite(result.date_image, sizeof(uint8_t), result.date_image_size, date_image_file_stream);
		if (written != result.date_image_size) {
			log_message(LOG_HEADER"Save worker %d failed with error couldn't write the complete date image", worker_info->worker_id, worker_info->worker_id, info.commit_hash);
		}
		fclose(date_image_file_stream);
		free(date_save_path);

		push_completed_frame(info);
		main_thread_post((struct main_thread_work) { .func = collect_backup_stats });
	}
	return NULL;
}
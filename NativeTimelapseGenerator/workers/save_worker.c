#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "worker_enums.h"
#include "worker_structs.h"
#include "save_worker.h"

#include "../console.h"
#include "../main_thread.h"
#include "../memory_utils.h"

#define LOG_HEADER "[save worker %d] "

// Reusable file-saving function
static int save_file(const char* path, const uint8_t* data, size_t size, char** error_msg)
{
	FILE* file = fopen(path, "wb");
	if (!file) {
		if (error_msg) {
			asprintf(error_msg, "Couldn't open file %s for writing", path);
		}
		return -1;
	}

	size_t written = fwrite(data, sizeof(uint8_t), size, file);
	fclose(file);

	if (written != size) {
		if (error_msg) {
			asprintf(error_msg, "Couldn't write the complete file %s", path);
		}
		return -1;
	}

	return 0;
}

SaveResult save(RenderResult render_result)
{
	SaveResult result = {
		.canvas_info = render_result.canvas_info,
		.download_result = render_result.download_result,
		.render_result = render_result,
		.save_error = SAVE_ERROR_NONE,
		.error_msg = NULL
	};

	char timestamp[64];
	struct tm timeinfo;
	if (gmtime_r(&render_result.canvas_info.date, &timeinfo) == NULL) {
		result.save_error = SAVE_ERROR_FAIL;
		result.error_msg = strdup("Failed to convert time to GMT");
		return result;
	}
	if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo) == 0) {
		result.save_error = SAVE_ERROR_FAIL;
		result.error_msg = strdup("Failed to format timestamp");
		return result;
	}

	// Save canvas image
	AUTOFREE char* save_path = NULL;
	asprintf(&save_path, "backups/%s.png", timestamp);
	if (save_file(save_path, render_result.canvas_image, render_result.canvas_image_size, &result.error_msg) != 0) {
		result.save_error = SAVE_ERROR_FAIL;
		return result;
	}

	// Save date image
	asprintf(&save_path, "dates/%s.png", timestamp);
	if (save_file(save_path, render_result.date_image, render_result.date_image_size, &result.error_msg) != 0) {
		result.save_error = SAVE_ERROR_FAIL;
		return result;
	}

	// Save top placers image
	asprintf(&save_path, "top_placers/%s.png", timestamp);
	if (save_file(save_path, render_result.top_placers_image, render_result.top_placers_image_size, &result.error_msg) != 0) {
		result.save_error = SAVE_ERROR_FAIL;
		return result;
	}

	// Save canvas control image
	asprintf(&save_path, "canvas_controls/%s.png", timestamp);
	if (save_file(save_path, render_result.canvas_control_image, render_result.canvas_control_image_size, &result.error_msg) != 0) {
		result.save_error = SAVE_ERROR_FAIL;
		return result;
	}

	return result;
}

void* start_save_worker(void* data)
{
	const WorkerInfo* worker_info = (const WorkerInfo*) data;

	log_message(LOG_INFO, LOG_HEADER"Started save worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter save loop
	while (true) {
		RenderResult render_result = pop_save_stack(worker_info->worker_id);

		SaveResult result = save(render_result);
		if (result.save_error != SAVE_ERROR_NONE) {
			log_message(LOG_ERROR, LOG_HEADER"Save worker %d failed with error %d message %s",
						worker_info->worker_id, result.save_error, result.error_msg);
			free(result.error_msg);
			continue;
		}

		push_completed(result);
	}
	return NULL;
}

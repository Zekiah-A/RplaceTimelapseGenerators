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
#include "../database.h"

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

SaveResult save(SaveJob job)
{
	char timestamp[64];
	struct tm timeinfo;
	if (gmtime_r(&job.date, &timeinfo) == NULL) {
		return (SaveResult) { .save_error = SAVE_ERROR_DATETIME, .error_msg = strdup("Failed to convert time to GMT") };
	}
	if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &timeinfo) == 0) {
		return (SaveResult) { .save_error = SAVE_ERROR_DATETIME, .error_msg = strdup("Failed to format timestamp") };
	}

	char* save_path = NULL;
	switch (job.type) {
		case SAVE_PLACERS_DOWNLOAD: {
			asprintf(&save_path, "placer_downloads/%s_%d_%s", timestamp, job.commit_id, job.commit_hash);
			break;
		}
		case SAVE_CANVAS_DOWNLOAD: {
			asprintf(&save_path, "canvas_downloads/%s_%d_%s", timestamp, job.commit_id, job.commit_hash);
			break;
		}
		case SAVE_CANVAS_RENDER: {
			asprintf(&save_path, "canvas_renders/%s_%d_%s.png", timestamp, job.commit_id, job.commit_hash);
			break;
		}
		case SAVE_DATE_RENDER: {
			asprintf(&save_path, "date_renders/%s_%d_%s.png", timestamp, job.commit_id, job.commit_hash);
			break;
		}
		case SAVE_TOP_PLACERS_RENDER: {
			asprintf(&save_path, "top_placer_renders/%s_%d_%s.png", timestamp, job.commit_id, job.commit_hash);
			break;
		}
		case SAVE_CANVAS_CONTROL_RENDER: {
			asprintf(&save_path, "canvas_control_renders/%s_%d_%s.png", timestamp, job.commit_id, job.commit_hash);
			break;
		}
		default: {
			return (SaveResult) { .save_error = SAVE_FAIL_TYPE, .error_msg = strdup("Invalid save job type") };
		}
	}

	// Save file locally  to filesystem
	char* error_msg = NULL;
	if (save_file(save_path, job.data, job.size, &error_msg) != 0) {
		return (SaveResult) { .save_error = SAVE_ERROR_FILESYSTEM, .error_msg = error_msg };
	}
	// Ensure saves are written to database
	if (!add_save_to_db(job.commit_id, job.type, save_path)) {
		return (SaveResult) { .save_error = SAVE_ERROR_DATABASE, .error_msg = strdup("Failed to write save to database") };
	}

	SaveResult result = {
		// Inherited from WorkerResult
		.save_error = SAVE_ERROR_NONE,
		.error_msg = NULL,
		// Inherited from CommitInfo
		.commit_id = job.commit_id,
		.commit_hash = job.commit_hash,
		.date = job.date,
		// Members
		.save_type = job.type,
		.save_path = save_path
	};
	return result;
}

void* start_save_worker(void* data)
{
	const WorkerInfo* worker_info = (const WorkerInfo*) data;

	log_message(LOG_INFO, LOG_HEADER"Started save worker with thread id %d",
		worker_info->worker_id, worker_info->thread_id);

	// Enter save loop
	while (true) {
		SaveJob job = pop_save_stack(worker_info->worker_id);

		SaveResult result = save(job);
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

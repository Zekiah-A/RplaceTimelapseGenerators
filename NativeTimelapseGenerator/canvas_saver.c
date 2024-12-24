#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_saver.h"
#include "console.h"
#include "main_thread.h"
#include "worker_structs.h"

#define LOG_HEADER "[save worker %d]"

void* start_save_worker(void* data)
{
	WorkerInfo* worker_info = (WorkerInfo*) data;
	worker_info->save_worker_data = (struct save_worker_data*) malloc(sizeof(struct save_worker_data));
	worker_info->save_worker_data->current_canvas_result = NULL;
	log_message(LOG_HEADER"Started save worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	// Enter save loop
	while (true)
	{
		RenderResult result = pop_save_stack(worker_info->worker_id);
		CanvasInfo info = result.canvas_info;

		FILE* image_file_stream = fopen(result.canvas_info.save_path, "wb");
		if (!image_file_stream)
		{
			log_message(LOG_HEADER"Save worker %s failed with error couldn't open file for writing", worker_info->worker_id, info.commit_hash);
			continue;
		}
		fwrite(result.image, sizeof(uint8_t), result.image_size, image_file_stream);
		fclose(image_file_stream);

		push_completed_frame(info);
		main_thread_post((struct main_thread_work) { .func = collect_backup_stats });
	}

	return NULL;
}
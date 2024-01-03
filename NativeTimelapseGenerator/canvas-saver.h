#pragma once
struct save_worker_data
{
    struct render_result* current_canvas_result;
};

void* start_save_worker(void* data);
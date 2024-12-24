// Modified code from rplace bot https://github.com/Zekiah-A/RplaceBot/blob/main/main.c
#include <stdio.h>
#include <png.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include "image_generator.h"
#include "main_thread.h"
#include "console.h"
#include "worker_structs.h"

#define LOG_HEADER "[render worker %d] "

Colour default_palette[32] = {
	{109, 0, 26},
	{190, 0, 57},
	{255, 69, 0},
	{255, 168, 0},
	{255, 214, 53},
	{255, 248, 184},
	{0, 163, 104},
	{0, 204, 120},
	{126, 237, 86},
	{0, 117, 111},
	{0, 158, 170},
	{0, 204, 192},
	{36, 80, 164},
	{54, 144, 234},
	{81, 233, 244},
	{73, 58, 193},
	{106, 92, 255},
	{148, 179, 255},
	{129, 30, 159},
	{180, 74, 192},
	{228, 171, 255},
	{222, 16, 127},
	{255, 56, 129},
	{255, 153, 170},
	{109, 72, 47},
	{156, 105, 38},
	{255, 180, 112},
	{0, 0, 0},
	{81, 82, 82},
	{137, 141, 144},
	{212, 215, 217},
	{255, 255, 255}
};

RenderResult generate_canvas_image(int width, int height, uint8_t* board, int palette_size, Colour* palette)
{
	RenderResult result = { .error = GENERATION_ERROR_NONE, .error_msg = NULL };
	if (width == 0 || height == 0) {
		result.error = GENERATION_FAIL_DRAW;
		result.error_msg = strdup("Canvas width or height was zero");
		return result;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		result.error = GENERATION_FAIL_DRAW;
		result.error_msg = strdup("PNG create write struct failed. png_ptr was null");
		png_destroy_write_struct(&png_ptr, NULL);
		return result;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		result.error = GENERATION_FAIL_DRAW;
		result.error_msg = strdup("PNG create info struct failed. info_ptr was null");
		png_destroy_write_struct(&png_ptr, NULL);
		return result;
	}

	char* stream_buffer = NULL;
	size_t stream_length = 0;
	FILE* memory_stream = open_memstream(&stream_buffer, &stream_length);

	png_init_io(png_ptr, memory_stream);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	png_bytep row_pointers[height];
	
	// Use default palette if provided palette is null or size is zero
	if (palette == NULL || palette_size == 0) {
		palette = default_palette;
		palette_size = 32;
	}

	// Transform byte array data into PNG
	for (int y = 0; y < height; y++) {
		row_pointers[y] = (png_bytep) calloc(sizeof(Colour) * width, sizeof(png_byte));
		for (int x = 0; x < width; x++) {
			int index = y * width + x;
			uint8_t colour_index = board[index];
			if (colour_index >= palette_size) {
				colour_index = 0;
			}
			for (int p = 0; p < sizeof(Colour); p++) { // r g b colour components
				row_pointers[y][sizeof(Colour) * x + p] = palette[colour_index].items[p];
			}
		}
	}

	png_write_image(png_ptr, row_pointers);

	for (int i = 0; i < height; i++) {
		free(row_pointers[i]);
	}
	png_write_end(png_ptr, NULL);

	fflush(memory_stream);
	fclose(memory_stream);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	
	result.image = (uint8_t*) stream_buffer;
	result.image_size = stream_length;
	return result;
}

void* start_render_worker(void* data)
{
	WorkerInfo* worker_info = (WorkerInfo*) data;
	worker_info->render_worker_data = (struct render_worker_data*) malloc(sizeof(struct render_worker_data));
	worker_info->render_worker_data->current_canvas_result = NULL;
	log_message(LOG_HEADER"Started render worker with thread id %d", worker_info->worker_id, worker_info->thread_id);

	while (true)
	{
		DownloadedResult download_result = pop_render_stack(worker_info->worker_id);
		CanvasInfo info = download_result.canvas_info;
		RenderResult result = generate_canvas_image(download_result.width, download_result.height,
			download_result.canvas, download_result.palette_size, download_result.palette);
		free(download_result.canvas); // safe to delete data from last step now we don't need it
		if (result.error)
		{
			log_message(LOG_HEADER"Render %s failed with error %d message %s", worker_info->worker_id, info.commit_hash, result.error, result.error_msg);
			continue;
		}
		else
		{
			result.canvas_info = download_result.canvas_info;
			push_save_stack(result);
		}
	}
	return NULL;
}
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
#include <cairo/cairo.h>

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

RenderResult generate_date_image(int width, const char* date_text)
{
	RenderResult result = { .error = GENERATION_ERROR_NONE, .error_msg = NULL };
	int text_image_height = 128;
	int text_image_width = width;
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, text_image_width, text_image_height);
	cairo_t* cr = cairo_create(surface);

	// Set transparent background
	cairo_set_source_rgba(cr, 0, 0, 0, 0); // transparent
	cairo_paint(cr);

	// Set text color and font
	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 32);

	// Calculate text position
	cairo_text_extents_t extents;
	cairo_text_extents(cr, date_text, &extents);
	double x = (text_image_width - extents.width) / 2;
	double y = (text_image_height - extents.height) / 2 + extents.height;

	// Draw drop shadow
	cairo_set_source_rgba(cr, 0, 0, 0, 0.5); // black with 50% opacity
	cairo_move_to(cr, x + 2, y + 2); // offset for shadow
	cairo_show_text(cr, date_text);

	// Draw text
	cairo_set_source_rgb(cr, 1, 1, 1); // white
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, date_text);

	// Finish drawing
	cairo_destroy(cr);

	// Write to memory buffer
	unsigned char* buffer = NULL;
	unsigned long buffer_size = 0;
	FILE* memory_stream = open_memstream((char**) &buffer, &buffer_size);
	if (memory_stream == NULL) {
		result.error = GENERATION_FAIL_DRAW;
		result.error_msg = strdup("Failed to open memory stream");
		cairo_surface_destroy(surface);
		return result;
	}
	if (cairo_surface_write_to_png_stream(surface, (cairo_write_func_t)fwrite, memory_stream) != CAIRO_STATUS_SUCCESS) {
		result.error = GENERATION_FAIL_DRAW;
		result.error_msg = strdup("Failed to write to memory stream");
		fclose(memory_stream);
		cairo_surface_destroy(surface);
		return result;
	}
	fclose(memory_stream);
	cairo_surface_destroy(surface);

	result.date_image = buffer;
	result.date_image_size = buffer_size;

	return result;
}

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

	while (true) {
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

		char date_text[32];
		strftime(date_text, sizeof(date_text), "%a %d %b %Y %H:%M", gmtime(&info.date));
		RenderResult date_result = generate_date_image(download_result.width, date_text);
		result.date_image = date_result.date_image;
		result.date_image_size = date_result.date_image_size;
		result.canvas_info = download_result.canvas_info;
		push_save_stack(result);
	}
	return NULL;
}
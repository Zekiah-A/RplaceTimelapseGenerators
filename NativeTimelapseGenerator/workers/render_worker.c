#include <cairo/cairo-deprecated.h>
#include <math.h>
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
#include <cairo/cairo.h>

#include "worker_enums.h"
#include "worker_structs.h"
#include "render_worker.h"

#include "../console.h"
#include "../main_thread.h"
#include "../memory_utils.h"
#include "../lib/stb/stb_ds.h"

#define LOG_HEADER "[render worker %d] "

Colour default_palette[32] = {
	{ .r = 109, .g = 0, .b = 26, .a = 255 },
	{ .r = 190, .g = 0, .b = 57, .a = 255 },
	{ .r = 255, .g = 69, .b = 0, .a = 255 },
	{ .r = 255, .g = 168, .b = 0, .a = 255 },
	{ .r = 255, .g = 214, .b = 53, .a = 255 },
	{ .r = 255, .g = 248, .b = 184, .a = 255 },
	{ .r = 0, .g = 163, .b = 104, .a = 255 },
	{ .r = 0, .g = 204, .b = 120, .a = 255 },
	{ .r = 126, .g = 237, .b = 86, .a = 255 },
	{ .r = 0, .g = 117, .b = 111, .a = 255 },
	{ .r = 0, .g = 158, .b = 170, .a = 255 },
	{ .r = 0, .g = 204, .b = 192, .a = 255 },
	{ .r = 36, .g = 80, .b = 164, .a = 255 },
	{ .r = 54, .g = 144, .b = 234, .a = 255 },
	{ .r = 81, .g = 233, .b = 244, .a = 255 },
	{ .r = 73, .g = 58, .b = 193, .a = 255 },
	{ .r = 106, .g = 92, .b = 255, .a = 255 },
	{ .r = 148, .g = 179, .b = 255, .a = 255 },
	{ .r = 129, .g = 30, .b = 159, .a = 255 },
	{ .r = 180, .g = 74, .b = 192, .a = 255 },
	{ .r = 228, .g = 171, .b = 255, .a = 255 },
	{ .r = 222, .g = 16, .b = 127, .a = 255 },
	{ .r = 255, .g = 56, .b = 129, .a = 255 },
	{ .r = 255, .g = 153, .b = 170, .a = 255 },
	{ .r = 109, .g = 72, .b = 47, .a = 255 },
	{ .r = 156, .g = 105, .b = 38, .a = 255 },
	{ .r = 255, .g = 180, .b = 112, .a = 255 },
	{ .r = 0, .g = 0, .b = 0, .a = 255 },
	{ .r = 81, .g = 82, .b = 82, .a = 255 },
	{ .r = 137, .g = 141, .b = 144, .a = 255 },
	{ .r = 212, .g = 215, .b = 217, .a = 255 },
	{ .r = 255, .g = 255, .b = 255, .a = 255 }
};

struct image_result {
	uint8_t* data;
	size_t size;
	RenderError error;
	// Has to be malloc allocated
	char* error_msg;
};

cairo_status_t cairo_write(void* closure, const unsigned char* data, unsigned int length)
{
    FILE* stream = (FILE*) closure;
    if (fwrite(data, 1, length, stream) != length) {
        return CAIRO_STATUS_WRITE_ERROR;
    }

    return CAIRO_STATUS_SUCCESS;
}

struct image_result generate_top_placers_image(Placer* top_placers, size_t top_placers_size)
{
	struct image_result result = {
		.error = RENDER_ERROR_NONE,
		.error_msg = NULL
	};
	int font_size = 96;
	int text_image_width = 1280;
	int text_image_height = font_size * top_placers_size;
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, text_image_width, text_image_height);
	cairo_t* cr = cairo_create(surface);

	// Transparent background
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	// Set text color and font
	cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, font_size);

	// Enable grayscale antialiasing
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_GRAY);

	// Draw text
	for (int i = 0; i < top_placers_size; i++) {
		Placer placer = top_placers[i];
		cairo_set_source_rgb(cr, placer.colour.r, placer.colour.g, placer.colour.b); // Placer colour

		AUTOFREE char* top_placer_text = NULL;
		asprintf(&top_placer_text, "%s (#%d) : %d pixels", placer.chat_name, placer.int_id, placer.pixels_placed);

		cairo_move_to(cr, 0,  i * font_size);
		cairo_show_text(cr, top_placer_text);
	}

	return result;
}

struct image_result generate_canvas_control_image(int width, int height, uint32_t* placers, Placer* top_placers, int top_placers_size)
{
	struct image_result result = { .error = RENDER_ERROR_NONE, .error_msg = NULL };
	if (width == 0 || height == 0) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("Placers width or height was zero");
		return result;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("PNG create write struct failed. png_ptr was null");
		png_destroy_write_struct(&png_ptr, NULL);
		return result;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("PNG create info struct failed. info_ptr was null");
		png_destroy_write_struct(&png_ptr, NULL);
		return result;
	}

	char* stream_buffer = NULL;
	size_t stream_length = 0;
	FILE* memory_stream = open_memstream(&stream_buffer, &stream_length);
	if (memory_stream == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("Failed to open canvas image memory stream");
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return result;
	}

	png_init_io(png_ptr, memory_stream);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	png_bytep row_pointers[height];
	
	// Transform byte array data into PNG
	for (int y = 0; y < height; y++) {
		row_pointers[y] = (png_bytep) calloc(sizeof(Colour) * width, sizeof(png_byte));
		for (int x = 0; x < width; x++) {
			int index = y * width + x;
			uint32_t user_int_id = placers[index];

			Placer top_placer = { 0 };
			for (int i = 0; i < top_placers_size; i++) {
				if (top_placers[i].int_id == user_int_id) {
					top_placer = top_placers[i];
				}
			}

			for (size_t p = 0; p < sizeof(Colour); p++) { // r g b colour components
				row_pointers[y][sizeof(Colour) * x + p] = top_placer.colour.channels[p];
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
	
	result.data = (uint8_t*) stream_buffer;
	result.size = stream_length;
	return result;
}

struct image_result generate_date_image(time_t date, int style)
{
	struct image_result result = { .error = RENDER_ERROR_NONE, .error_msg = NULL };

	char date_text[64];
	strftime(date_text, sizeof(date_text), "%a %d %b %Y %H:%M", gmtime(&date));

	int text_image_width = 1280;
	int text_image_height = 128;
	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, text_image_width, text_image_height);
	cairo_t* cr = cairo_create(surface);

	if (style == 0) {
		// Transparent background
		cairo_set_source_rgba(cr, 0, 0, 0, 0);
		cairo_paint(cr);

		// Set text color and font
		cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 96);

		// Enable grayscale antialiasing
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_GRAY);

		// Calculate text position with proper alignment
		cairo_text_extents_t extents;
		cairo_text_extents(cr, date_text, &extents);
		double x = floor((text_image_width - extents.width) / 2 - extents.x_bearing);
		double y = floor((text_image_height - extents.height) / 2 - extents.y_bearing);

		// Draw drop shadow
		cairo_set_source_rgba(cr, 0, 0, 0, 0.5); // Black with 50% opacity
		cairo_move_to(cr, x + 2, y + 2); // Offset for shadow
		cairo_show_text(cr, date_text);

		// Draw text
		cairo_set_source_rgb(cr, 1, 1, 1); // White
		cairo_move_to(cr, x, y);
		cairo_show_text(cr, date_text);
	}
	else if (style == 1) {
		// Transparent background
		cairo_set_source_rgba(cr, 0, 0, 0, 0);
		cairo_paint(cr);

		// Set font and font size
		cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 96);

		// Enable grayscale antialiasing
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_GRAY);

		// Calculate text position with proper alignment
		cairo_text_extents_t extents;
		cairo_text_extents(cr, date_text, &extents);
		double x = floor((text_image_width - extents.width) / 2 - extents.x_bearing);
		double y = floor((text_image_height - extents.height) / 2 - extents.y_bearing);

		// Draw glow effect by layering text with decreasing opacity
		for (int i = 6; i > 0; i--) {
			cairo_set_source_rgba(cr, 0, 1, 0, 0.05 + 0.1 * (6 - i));
			cairo_move_to(cr, x, y + i * 2);
			cairo_show_text(cr, date_text);
		}

		// Draw main text
		cairo_set_source_rgb(cr, 0, 1, 0);
		cairo_move_to(cr, x, y);
		cairo_show_text(cr, date_text);
	}

	// Finish drawing
	cairo_destroy(cr);

	// Write to memory buffer
	unsigned char* buffer = NULL;
	unsigned long buffer_size = 0;
	FILE* memory_stream = open_memstream((char**) &buffer, &buffer_size);
	if (memory_stream == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("Failed to open date image memory stream");
		perror("open_memstream failed");
		cairo_surface_destroy(surface);
		return result;
	}
	if (cairo_surface_write_to_png_stream(surface, cairo_write, memory_stream) != CAIRO_STATUS_SUCCESS) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("Failed to write to date image memory stream");
		fclose(memory_stream);
		cairo_surface_destroy(surface);
		return result;
	}
	fclose(memory_stream);
	cairo_surface_destroy(surface);

	result.data = buffer;
	result.size = buffer_size;
	return result;
}

struct image_result generate_canvas_image(int width, int height, uint8_t* board, int palette_size, Colour* palette)
{
	struct image_result result = {
		.error = RENDER_ERROR_NONE,
		.error_msg = NULL
	};
	if (width == 0 || height == 0) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("Board width or height was zero");
		return result;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("PNG create write struct failed. png_ptr was null");
		png_destroy_write_struct(&png_ptr, NULL);
		return result;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("PNG create info struct failed. info_ptr was null");
		png_destroy_write_struct(&png_ptr, NULL);
		return result;
	}

	char* stream_buffer = NULL;
	size_t stream_length = 0;
	FILE* memory_stream = open_memstream(&stream_buffer, &stream_length);
	if (memory_stream == NULL) {
		result.error = RENDER_FAIL_DRAW;
		result.error_msg = strdup("Failed to open canvas image memory stream");
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return result;
	}

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
				row_pointers[y][sizeof(Colour) * x + p] = palette[colour_index].channels[p];
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
	
	result.data = (uint8_t*) stream_buffer;
	result.size = stream_length;
	return result;
}

RenderResult render(RenderJob job)
{
	SaveJobType save_type = { 0};
	struct image_result image = { 0 };

	switch (job.type) {
		case RENDER_CANVAS: {
			image = generate_canvas_image(job.canvas.width, job.canvas.height,
				job.canvas.data, job.canvas.size, job.canvas.palette);
			if (image.error != RENDER_ERROR_NONE) {
				return (RenderResult) { .render_error = image.error, .error_msg = image.error_msg };
			}
			save_type = SAVE_CANVAS_RENDER;
			break;
		}
		case RENDER_DATE: {
			image = generate_date_image(job.date, 0);
			if (image.error != RENDER_ERROR_NONE) {
				return (RenderResult) { .render_error = image.error, .error_msg = image.error_msg };
			}
			save_type = SAVE_DATE_RENDER;
			break;
		}
		case RENDER_TOP_PLACERS: {
			image = generate_top_placers_image(
				job.top_placers.top_placers, job.top_placers.top_placers_size);
			if (image.error != RENDER_ERROR_NONE) {
				return (RenderResult) { .render_error = image.error, .error_msg = image.error_msg };
			}
			save_type = SAVE_TOP_PLACERS_RENDER;
			break;
		}
		case RENDER_CANVAS_CONTROL: {
			image = generate_canvas_control_image(job.canvas_control.width, job.canvas_control.height,
				job.canvas_control.placers, job.canvas_control.top_placers, job.canvas_control.top_placers_size);
			if (image.error != RENDER_ERROR_NONE) {
				return (RenderResult) { .render_error = image.error, .error_msg = image.error_msg };
			}
			save_type = SAVE_CANVAS_CONTROL_RENDER;
			break;
		}
		default: {
			return (RenderResult) { .render_error = RENDER_FAIL_TYPE, .error_msg = strdup("Invalid render job type") };
		}
	}

	RenderResult result = {
		// Inherited from WorkerResult
		.render_error = RENDER_ERROR_NONE,
		.error_msg = NULL,
		// Members
		.save_job = {
			// Inherited from WorkerJob
			.commit_id = job.commit_id,
			.commit_hash = job.commit_hash,
			.date = job.date,
			// Members
			.type = save_type,
			.data = image.data,
			.size = image.size
		}
	};
	return result;
}

void* start_render_worker(void* data)
{
	// Initialise worker / thread globals
	const WorkerInfo* worker_info = (const WorkerInfo*) data;

	log_message(LOG_INFO, LOG_HEADER"Started render worker with thread id %d",
		worker_info->worker_id, worker_info->thread_id);

	// Enter render loop
	while (true) {
		RenderJob job = pop_render_stack(worker_info->worker_id);

		RenderResult result = render(job);
		if (result.render_error != RENDER_ERROR_NONE) {
			log_message(LOG_ERROR, LOG_HEADER"Render %s failed with error %d message %s",
				worker_info->worker_id, job.commit_hash, result.render_error, result.error_msg);
			free(result.error_msg);
			continue;
		}
		push_save_stack(result.save_job);
	}
	return NULL;
}

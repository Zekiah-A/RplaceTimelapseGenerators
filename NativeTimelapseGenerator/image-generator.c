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
#include "image-generator.h"
#include "main-thread.h"
#include "console.h"

#define LOG_HEADER "[render worker] "

uint8_t default_palette[32][3] = {
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


struct render_result generate_canvas_image(int width, int height, struct region_info region, uint8_t* board, int size)
{
    struct render_result gen_result = { .error = GENERATION_ERROR_NONE, .error_msg = NULL };
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL)
    {
        gen_result.error = GENERATION_FAIL_DRAW;
        gen_result.error_msg = "PNG create write struct failed. png_ptr was null";
        png_destroy_write_struct(&png_ptr, NULL);
        return gen_result;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        gen_result.error = GENERATION_FAIL_DRAW;
        gen_result.error_msg = "PNG create info struct failed. info_ptr was null";
        png_destroy_write_struct(&png_ptr, NULL);
        return gen_result;
    }

    char* stream_buffer = NULL;
    size_t stream_length = 0;
    FILE* memory_stream = open_memstream(&stream_buffer, &stream_length);

    png_init_io(png_ptr, memory_stream);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    png_bytep row_pointers[height];
    
    // Okay now we can actually draw
    for (int i = 0; i < height; i++) {
        row_pointers[i] = (png_bytep) calloc(3 * width, sizeof(png_byte));
        for (int j = 0; j < width; j++) {
            int index = i * width + j;
            for (int p = 0; p < 3; p++) {
                row_pointers[i][3 * j] = default_palette[board[i]][p]; // colour
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
    
    gen_result.data = (uint8_t*) stream_buffer;
    gen_result.length = stream_length;
    return gen_result;
}

void* start_render_worker(void* data)
{
    WorkerInfo* worker_info = (WorkerInfo*) data;
    worker_info->render_worker_data = (struct render_worker_data*) malloc(sizeof(struct render_worker_data));
    worker_info->render_worker_data->current_canvas_result = NULL;
    log_message(LOG_HEADER"Started render worker with thread id %d", worker_info->thread_id);

    while (1)
    {

    }
    return NULL;
}
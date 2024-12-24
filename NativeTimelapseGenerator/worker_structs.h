#pragma once
#include "worker_enums.h"
#include <stdint.h>
#include <time.h>

typedef uint8_t Colour[3];

typedef struct canvas_info {
	char* base_url;
	char* commit_hash;
	char* save_path;
	time_t date;
} CanvasInfo;

typedef struct downloaded_result {
	CanvasInfo canvas_info;
	DownloadError error;
	int width;
	int height;
	int palette_size;
	Colour* palette;
	int canvas_size;
	uint8_t* canvas;
	// must be mutable, may be freed by error handler
	char* error_msg;
} DownloadedResult;

typedef struct render_result {
	CanvasInfo canvas_info;
	GenerationError error;
	int image_size;
	uint8_t* image;
	// must be mutable, may be freed by error handler
	char* error_msg;
} RenderResult;
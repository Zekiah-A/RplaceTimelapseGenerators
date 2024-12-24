#pragma once
#include "worker_enums.h"
#include <stdint.h>
#include <time.h>

typedef union {
	uint8_t items[4];
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
} Colour;

typedef struct canvas_info {
	char* commit_hash;
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
	int date_image_size;
	uint8_t* date_image;
	// must be mutable, may be freed by error handler
	char* error_msg;
} RenderResult;
#pragma once
#include <stdint.h>
#include <time.h>

#include "worker_enums.h"

// Cross-worker structs

typedef union {
	uint8_t channels[4];
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
} Colour;

typedef struct user {
	uint32_t int_id;
	// Can be null
	const char* chat_name;
	uint32_t pixels_placed;
	uint32_t play_time_seconds;
	time_t last_joined;
} User;

typedef struct placer {
	uint32_t int_id;
	// Can be null
	const char* chat_name;
	uint32_t pixels_placed;
} Placer;


// Pipeline worker structs

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
	size_t canvas_size;
	uint8_t* canvas;
	size_t placers_size;
	uint32_t* placers;
	size_t top_placers_size;
	Placer* top_placers;
	// must be mutable, may be freed by error handler
	char* error_msg;
} DownloadResult;

typedef struct render_result {
	CanvasInfo canvas_info;
	DownloadResult download_result;

	GenerationError error;
	int image_size;
	uint8_t* image;
	int date_image_size;
	uint8_t* date_image;
	// must be mutable, may be freed by error handler
	char* error_msg;
} RenderResult;

typedef struct save_result {
	CanvasInfo canvas_info;
	DownloadResult download_result;
	RenderResult render_result;
	// must be mutable, may be freed by error handler
	char* error_msg;
} SaveResult;
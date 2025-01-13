#pragma once
#include <stdint.h>
#include <time.h>

#include "worker_enums.h"

// Cross-worker structs
typedef union colour {
	uint8_t channels[4];
	struct {
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
	uint32_t value;
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
	Colour colour;
} Placer;


// Pipeline worker structs
typedef struct error {
	union {
		DownloadError download_error;
		RenderError render_error;
		SaveError save_error;
	};
	// Must be mutable, may be freed by error handler
	char* error_msg;
} Error;

typedef struct canvas_info {
	char* commit_hash;
	time_t date;
} CanvasInfo;

typedef struct downloaded_result {
	Error;
	CanvasInfo canvas_info;

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
} DownloadResult;

typedef struct render_result {
	Error;
	CanvasInfo canvas_info;
	DownloadResult download_result;

	uint8_t* canvas_image;
	int canvas_image_size;
	uint8_t* date_image;
	int date_image_size;
	uint8_t* top_placers_image;
	int top_placers_image_size;
	uint8_t* canvas_control_image;
	int canvas_control_image_size;
} RenderResult;

typedef struct save_result {
	Error;
	CanvasInfo canvas_info;
	DownloadResult download_result;
	RenderResult render_result;
} SaveResult;
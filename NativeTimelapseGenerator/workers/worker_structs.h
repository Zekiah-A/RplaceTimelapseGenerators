#pragma once
#include <stdint.h>
#include <time.h>

#include "worker_enums.h"

// Shared structs
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

typedef struct canvas_metadata {
	int width;
	int height;
	Colour* palette;
	int palette_size;
} CanvasMetadata;

typedef struct canvas_info {
	char* commit_hash;
	time_t date;
} CommitInfo;

// Base structs for worker jobs & results
typedef struct worker_result {
	union {
		DownloadError download_error;
		RenderError render_error;
		SaveError save_error;
	};
	// Must be mutable, may be freed by error handler
	char* error_msg;
} WorkerResult;

typedef struct worker_job {
	CommitInfo;
} WorkerJob;

// Main thread
typedef struct stats_job {
	WorkerJob;
	char* save_path;
} StatsJob;

// Save worker
typedef enum save_job_type {
	SAVE_CANVAS_DOWNLOAD = 0,
	SAVE_CANVAS_RENDER = 1,
	SAVE_DATE_RENDER = 2,
	SAVE_PLACERS_DOWNLOAD = 3,
	SAVE_TOP_PLACERS_RENDER = 4,
	SAVE_CANVAS_CONTROL_RENDER = 5
} SaveJobType;

typedef struct save_job {
	WorkerJob;
	SaveJobType type;
	uint8_t* data;
	size_t size;
} SaveJob;

typedef struct save_result {
	WorkerResult;
	StatsJob stats_job;
} SaveResult;

// Render worker
typedef enum render_job_type {
	RENDER_CANVAS,
	RENDER_DATE,
	RENDER_TOP_PLACERS,
	RENDER_CANVAS_CONTROL
} RenderJobType;

typedef struct render_job_canvas {
	int width;
	int height;
	int palette_size;
	Colour* palette;
	size_t size;
	uint8_t* data;
} RenderJobCanvas;

typedef struct render_job_top_placers {
	size_t top_placers_size;
	Placer* top_placers;
} RenderJobTopPlacers;

typedef struct render_job_canvas_control {
	RenderJobTopPlacers;
	int width;
	int height;
	size_t placers_size;
	uint32_t* placers;
} RenderJobCanvasControl;

typedef struct render_job {
	WorkerJob;
	RenderJobType type;
	union {
		RenderJobCanvas canvas;
		RenderJobTopPlacers top_placers;
		RenderJobCanvasControl canvas_control;
	};
} RenderJob;

typedef struct render_result {
	WorkerResult;
	SaveJob save_job;
} RenderResult;


// Download worker
typedef enum download_job_type {
	DOWNLOAD_CANVAS,
	DOWNLOAD_PLACERS
} DownloadJobType;

typedef struct download_job {
	WorkerJob;
	DownloadJobType type;
} DownloadJob;

typedef struct download_result {
	WorkerResult;
	RenderJob render_job;
} DownloadResult;

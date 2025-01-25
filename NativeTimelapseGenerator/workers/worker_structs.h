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
	int commit_id;
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
typedef enum job_type {
	JOB_TYPE_DOWNLOAD = 1,
	JOB_TYPE_RENDER = 2,
	JOB_TYPE_SAVE = 3
} JobType;

// Save worker
typedef enum save_job_type:uint8_t {
	SAVE_CANVAS_DOWNLOAD = 1,
	SAVE_CANVAS_RENDER = 2,
	SAVE_DATE_RENDER = 3,
	SAVE_PLACERS_DOWNLOAD = 4,
	SAVE_TOP_PLACERS_RENDER = 5,
	SAVE_CANVAS_CONTROL_RENDER = 6
} SaveJobType;

typedef struct save_job {
	WorkerJob;
	SaveJobType type;
	uint8_t* data;
	size_t size;
} SaveJob;

typedef struct save_result {
	WorkerResult;
	CommitInfo;
	SaveJobType save_type;
	char* save_path;
} SaveResult;

// Render worker
typedef enum render_job_type {
	RENDER_CANVAS = 1,
	RENDER_DATE = 2,
	RENDER_TOP_PLACERS = 3,
	RENDER_CANVAS_CONTROL = 4
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
	DOWNLOAD_CANVAS = 1,
	DOWNLOAD_PLACERS = 2
} DownloadJobType;

typedef struct download_job {
	WorkerJob;
	DownloadJobType type;
} DownloadJob;

typedef struct download_result {
	WorkerResult;
	JobType job_type;
	union {
		SaveJob save_job;
		RenderJob render_job;
	};
} DownloadResult;

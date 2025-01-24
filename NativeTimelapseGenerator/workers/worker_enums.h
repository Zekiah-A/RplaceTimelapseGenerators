#pragma once

typedef enum render_error
{
	RENDER_ERROR_NONE = 0,
	RENDER_FAIL_DRAW = 1,
	RENDER_FAIL_TYPE = 2
} RenderError;

typedef enum download_error {
	DOWNLOAD_ERROR_NONE = 0,
	DOWNLOAD_FAIL_FETCH = 1,
	DOWNLOAD_FAIL_BADFILE = 2,
	DOWNLOAD_FAIL_METADATA = 3,
	DOWNLOAD_FAIL_PLACERS = 4,
	DOWNLOAD_FAIL_TYPE = 5
} DownloadError;

typedef enum save_error {
	SAVE_ERROR_NONE = 0,
	SAVE_ERROR_FAIL = 1,
	SAVE_FAIL_TYPE = 2
} SaveError;
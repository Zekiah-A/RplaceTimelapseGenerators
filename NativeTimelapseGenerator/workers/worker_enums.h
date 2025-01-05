#pragma once

typedef enum generation_error
{
	GENERATION_ERROR_NONE = 0,
	GENERATION_FAIL_DRAW = 1
} GenerationError;

typedef enum download_error {
	DOWNLOAD_ERROR_NONE = 0,
	DOWNLOAD_FAIL_FETCH = 1,
	DOWNLOAD_FAIL_BADFILE = 2,
	DOWNLOAD_FAIL_METADATA = 3,
	DOWNLOAD_FAIL_PLACERS = 4
} DownloadError;
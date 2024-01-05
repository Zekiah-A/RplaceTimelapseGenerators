#pragma once
#include <stdint.h>
#include <time.h>

struct canvas_info
{
    char* url;
    char* commit_hash;
    time_t date;
};

struct downloaded_result
{
    struct canvas_info canvas_info;
    int size;
    uint8_t* data;
    int error;
    // must be mutable, may be freed by error handler
    char* error_msg;
};

struct render_result
{
    struct canvas_info canvas_info;
    int error;
    // must be mutable, may be freed by error handler
    char* error_msg;
    int length;
    uint8_t* data;
};
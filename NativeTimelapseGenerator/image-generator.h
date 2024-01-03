// Extracted code from rplace bot https://github.com/Zekiah-A/RplaceBot/blob/main/main.c
#include <stdint.h>

struct downloaded_backup {
    int size;
    uint8_t* data;
    int error;
    char* error_msg;
};

struct canvas_image {
    int error;
    char* error_msg;
    int length;
    uint8_t* data;
};

struct region_info {
    int start_x;
    int start_y;
    int scale;
};

struct canvas_image generate_canvas_image(int width, int height, struct region_info region, uint8_t* board, int size);

#define GENERATION_ERROR_NONE 0
#define GENERATION_FAIL_DRAW 1
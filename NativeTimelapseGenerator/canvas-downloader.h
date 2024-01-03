struct downloaded_result
{
    int error;
    char* error_msg;
    char* data;
};

struct downloaded_result download_url(const char* url);

#define DOWNLOAD_ERROR_NONE 0
#define DOWNLOAD_FAIL_FETCH 1
#include "console.h"
#include <linux/limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dlfcn.h"
#include "main-thread.h"

void* handle = NULL;
start_console_delegate start_console_cli = NULL;
log_message_delegate log_message_cli = NULL;
stop_console_delegate stop_console_cli = NULL;

//#ifdef DEBUG
//#define CONSOLE_LIB_NAME "NativeTimelapseGenerator.Console.so.dbg"
//#else
#define CONSOLE_LIB_NAME "NativeTimelapseGenerator.Console.so"
//#endif

// We still in UI thread, must pass back to main thread
void ui_start_generation()
{
    main_thread_post((struct main_thread_work) { .func = start_generation });
}

// We still in UI thread, must pass back to main thread
void ui_stop_generation()
{
    printf("Generator CLI application halted. Application will terminate immediately\n");
    main_thread_post((struct main_thread_work) { .func = stop_generation });
}

void* start_console(void* data)
{
    char lib_path[PATH_MAX];
    getcwd(lib_path, sizeof(lib_path));
    if (lib_path[strlen(lib_path) - 1] != '/')
    {
        strcat(lib_path, "/");
    }
    strcat(lib_path, CONSOLE_LIB_NAME);

    handle = dlopen(lib_path, RTLD_LAZY);
    start_console_cli = dlsym(handle, "start_console");
    log_message_cli = dlsym(handle, "log_message");
    stop_console_cli = dlsym(handle, "stop_console");
    if (start_console_cli == NULL || log_message_cli == NULL || stop_console_cli == NULL)
    {
        fprintf(stderr, "Error - Start console library couldn't be loaded! Method binding failed\n");
        exit(EXIT_FAILURE);
    }

    start_console_cli(&ui_start_generation, &ui_stop_generation);
    return NULL;
}

void stop_console()
{
    stop_console_cli();
}

void log_message(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    size_t size = vsnprintf(NULL, 0, format, args) + 1; // +1 for null-terminator
    va_end(args);

    char* buffer = (char*) malloc(size);
    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);

    if (log_message_cli != NULL)
    {
        log_message_cli(buffer);
    }

    free(buffer);
}
#include "console.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dlfcn.h"

typedef void (*start_console_delegate)(void*);

void handle_shutdown()
{
    puts("C Handle shutdown called"); 
}

void* start_console(void* data)
{
    char* console_lib_path = "/home/zekiah/RplaceTimelapseGenerators/NativeTimelapseGenerator.Console/bin/Release/net8.0/linux-x64/publish/NativeTimelapseGenerator.Console.so";
    void* handle = dlopen(console_lib_path, RTLD_LAZY);
    start_console_delegate start_console_cli = dlsym(handle, "start_console");
    if (start_console_cli == NULL)
    {
        puts("Error - Start console library couldn't be loaded!");
        exit(-1);
    }
    start_console_cli(&handle_shutdown);
    return NULL;
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

    //print_log(buffer);

    free(buffer);
}
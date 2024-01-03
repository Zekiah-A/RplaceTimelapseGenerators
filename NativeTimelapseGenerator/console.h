void log_message(const char* format, ...);
void* start_console(void* data);

// NAME: Start_console, ARGS: start_generation_callback, stop_generation_callback
typedef void (*start_console_delegate)(void*, void*);
// NAME: stop_console, ARGS: char* message
typedef void (*log_message_delegate)(char*);
#include <png.h>
#include <pthread.h>
#include <curl/curl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "console.h"
#include "main-thread.h"

// Main thread creates download worker, reads file incrementally, giving each download worker
// the corresponding canvas to download to memory.
// Download workers run, push curl results to the canvas queue,
// These are then processed by the render workers, which render out the canvases
// These are finally passed to save workers, which pull the results from the render workers and save to disk
int main(int argc, char* argv[])
{
    bool no_cli = argc > 1 && strcmp(argv[1], "--nocli") == 0;
    if (!no_cli)
    {
        // Start console thread
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, start_console, NULL); 
    }

    // Start main thread (will never return, and handle exiting itself)
    start_main_thread(no_cli);
    return 0;
}

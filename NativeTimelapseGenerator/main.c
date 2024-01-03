#include <stdio.h>
#include <png.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "console.h"

// Rplace timelapse generator, except in C! A plaintext list of all backups, separated by newline
// must be from git history (git clone https://rslashplace2/rslashplace2.github.io), (then in the root
// rplace directory), run git log --follow -- place > commit_hashes.txt), then placed in the folder of this.
// gcc -o main -lpng -lcurl main.c

#define THREAD_COUNT 4

pthread_t threads[THREAD_COUNT];
int threads_top = 0;
int threads_finished = 0;

struct render_task {
    char* lines;
    int length;
};

struct memory_fetch {
    size_t size;
    char* memory;
};

int width = 2000;
int height = 2000;
int backups_finished = 0;
char backups_dir[256];
pthread_mutex_t fetch_lock;

// Main thread creates download worker, reads file incrementally, giving each download worker
// the corresponding canvas to download to memory.
// Download workers run, push curl results to the canvas queue,
// These are then processed by the render workers, which render out the canvases
// These are finally passed to save workers, which pull the results from the render workers and save to disk

// Fetch methods
// - https://raw.githubusercontent.com/rslashplace2/rslashplace2.github.io/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place
// - https://github.com/rslashplace2/rslashplace2.github.io/blob/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place?raw=true
// - https://github.com/rslashplace2/rslashplace2.github.io/raw/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place
int flines(FILE* file) {
    char ch;
    int count;

    // Apparently getc just gets the next occurance of that char in the file
    for (ch = getc(file); ch != EOF; ch = getc(file)) {
        if (ch == '\n') {
            count++;
        }
    }

    return count;
}
/*
void* render_backup(void* args) {
    struct render_task* task = (struct render_task*) args;
    CURL *curl = curl_easy_init();
    CURLcode result;

    struct memory_fetch chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char line[41];
    line[40] = '\0';
    char url[117];

    for (int i = 0; i < task->length; i++) {
        memcpy(line, task->lines + (i * 40), 40);
        snprintf(url, sizeof(url), "https://raw.githubusercontent.com/rslashplace2/rslashplace2.github.io/%s/place", line);

        pthread_mutex_lock(&fetch_lock);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
        // curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "application/vnd.github.raw");
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_fetch);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

        result = curl_easy_perform(curl);
        pthread_mutex_unlock(&fetch_lock);

        if (result != CURLE_OK) {
            printf("%s\n", curl_easy_strerror(result));
            perror("Error fetching file");
            printf("%d\n", result);
            continue;
        }

        if (chunk.size < width * height) {
            printf("Canvas size was less than expected! %d\n", chunk.size);
            continue;
        }


        printf("Completed backup %s, finished %d.\n", line, backups_finished);
        backups_finished++;
    }

    threads_finished++;
    free(chunk.memory);
    curl_easy_cleanup(curl);
    return NULL;
}
*/

int main(int argc, char* argv[])
{
    // Start console
    pthread_t thread_id; 
    pthread_create(&thread_id, NULL, start_console, NULL); 

    // Start process
    curl_global_init(CURL_GLOBAL_DEFAULT);
    while (1) {
        sleep(1);
    }


    // Shutdown
    curl_global_cleanup();
    pthread_exit(NULL);
    return 0;
    /*
    char* hash = malloc(40);
    
    // Download commit hashes file
    for (int i = 0; i < argc; i++) {
        if (strcmp("--help", argv[i]) == 0) {
            printf("\x1b[32mRplace Timelapse Generator (c) Zekiah-A\x1b[0m\nRun this program with no arguments, but a supplied \x1b[1;4mcommit_hashes.txt\x1b[0m "
            "file to generate backups from all commit hashes.\n\nTo generate a backup from a single commit hash, run the "
            "program with the specified commit hash, i.e \x1b[1;4m./main 69190a70f0d116029f5cf882d9ab4e336ac42664\x1b[0m.\n\n"
            "To stop the program while it is generating backups, enter '\x1b[1;4mexit\x1b[0m', to gracefully terminate all threads.\x1b[0m\n\n");
            return 0;
        }
        else if(strlen(argv[i]) == 40) {
            memcpy(hash, argv[i], 40);
            hash[41] = '\0';
        }
    }

    // Setup backups dir variable
    getcwd(backups_dir, sizeof(backups_dir));
    strcat(backups_dir, "/backups/");
    mkdir(backups_dir, 0755);

    if (pthread_mutex_init(&fetch_lock, NULL) != 0)
    {
        printf("\x1b[1;31mError, fetch lock mutex init failed, try running again\n");
        return 1;
    }

    if (strlen(hash) > 0) {
        printf("Using provided commit hash, see --help for info on more commands.\n");
        struct render_task* task = malloc(sizeof(struct render_task));
        task->lines = hash;
        task->length = 1;

        pthread_create(threads + threads_top, NULL, render_backup, task);

        while (threads_finished == 0) {
            char input[256];
            scanf("%s", input);
            if (strcmp(input, "exit") == 0) {
                printf("Stopping\n");
                return 0;
            }
        }
    }
    else {
        printf("Using commit hashes file, see --help for info on more commands.\n");
        // Read list of all
        FILE* file = fopen("commit_hashes.txt", "r");
        if (file == NULL) {
            printf("\x1b[1;31mError, could not locate commit hashes file (commit_hashes.txt)\n");
            return 1;
        }

        long file_lines = 7957; //flines(file);
        char* lines = malloc(file_lines * 40);

        for (int i = 0; i < file_lines; i++) {
            char* line = NULL;
            unsigned long length = 0; // This should always be 40 anyway
            getline(&line, &length, file);
            memcpy(lines + (i * 40), line, 40);
        }

        // Split up lines into respective chunks, each thread handles a chunk to render
        int per_thread = file_lines / THREAD_COUNT;
        for (int i = 0; i < THREAD_COUNT; i++) {
            struct render_task* task = malloc(sizeof(struct render_task));
            task->lines = lines + (40 * per_thread * i);
            task->length = per_thread;

            pthread_create(threads + threads_top, NULL, render_backup, task);
            threads_top++;
        }

        // Pro move, actually let's make this unsafe too
        while (threads_finished < THREAD_COUNT) {
            char input[256];
            scanf("%s", input);
            if (strcmp(input, "exit") == 0) {
                printf("Stopping\n");
                return 0;
            }
        }
    }

    pthread_mutex_destroy(&fetch_lock);
    return 0;*/
}

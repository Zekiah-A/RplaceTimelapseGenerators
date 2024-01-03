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
#include "canvas-downloader.h"
#include "image-generator.h"
#include "main-thread.h"
#include "canvas-saver.h"

// Reads canvas list, manages jobs for workers

// SHARED BETWEEN-WORKER MEMORY
#define STACK_SIZE_MAX 100
#define DEFAULT_DOWNLOAD_WORKER_COUNT 2
struct canvas_info download_stack[STACK_SIZE_MAX];
int download_stack_top = 0;
#define DEFAULT_RENDER_WORKER_COUNT 4
struct downloaded_result render_stack[STACK_SIZE_MAX];
int render_stack_top = 0;
#define DEFAULT_SAVE_WORKER_COUNT 2
struct render_result save_stack[STACK_SIZE_MAX];
int save_stack_top = 0;

// WORKER THREADS
#define WORKER_MAX 100
struct worker_info* download_workers[WORKER_MAX] = { };
int download_worker_count = 0;
struct worker_info* render_workers[WORKER_MAX] = { };
int render_worker_count = 0;
struct worker_info* save_workers[WORKER_MAX] = { };
int save_worker_count = 0;

int width = 1000;
int height = 1000;
int backups_finished = 0;
char backups_dir[256];

pthread_mutex_t work_wait_mutex;

// Private
void init_work_queue(struct main_thread_queue* queue, size_t capacity)
{
    queue->work = (struct main_thread_work*) malloc(sizeof(struct main_thread_work) * capacity);
    if (!queue->work)
    {
        fprintf(stderr, "Failed to initialise main thread work queue\n");
        exit(EXIT_FAILURE);
    }

    queue->capacity = capacity;
    queue->front = 0;
    queue->rear = 0;
    pthread_mutex_init(&queue->mutex, NULL);
}

// Dequeue a message
void push_work_queue(struct main_thread_queue* queue, struct main_thread_work work)
{
    pthread_mutex_lock(&queue->mutex);

    // Check for queue overflow
    if ((queue->rear + 1) % queue->capacity == queue->front)
    {
        fprintf(stderr, "Error - Work queue overflow.\n");
        pthread_mutex_unlock(&queue->mutex);
        exit(EXIT_FAILURE);
    }

    // Enqueue the message
    queue->work[queue->rear] = work;
    queue->rear = (queue->rear + 1) % queue->capacity;

    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_unlock(&work_wait_mutex); 
}

struct main_thread_work pop_work_queue(struct main_thread_queue* queue)
{
    pthread_mutex_lock(&queue->mutex);

    // Check underflow
    if (queue->front == queue->rear)
    {
        fprintf(stderr, "Error - Work queue underflow.\n");
        pthread_mutex_unlock(&queue->mutex);
        exit(EXIT_FAILURE);
    }

    // Dequeue the message
    struct main_thread_work message = queue->work[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    pthread_mutex_unlock(&queue->mutex);

    return message;
}

void cleanup_work_queue(struct main_thread_queue* queue)
{
    free(queue->work);
    pthread_mutex_destroy(&queue->mutex);
}

// Fetch methods
// - https://raw.githubusercontent.com/rslashplace2/rslashplace2.github.io/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place
// - https://github.com/rslashplace2/rslashplace2.github.io/blob/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place?raw=true
// - https://github.com/rslashplace2/rslashplace2.github.io/raw/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place
int flines(FILE* file)
{
    char ch;
    int count;

    // Apparently getc just gets the next occurance of that char in the file
    for (ch = getc(file); ch != EOF; ch = getc(file))
    {
        if (ch == '\n')
        {
            count++;
        }
    }

    return count;
}

void add_download_worker()
{
    pthread_t thread_id;
    struct worker_info* info = (struct worker_info*) malloc(sizeof(struct worker_info));
    pthread_create(&thread_id, NULL, start_download_worker, info);
    info->thread_id = thread_id;
    download_workers[download_worker_count] = info;
    download_worker_count++;
}

void add_render_worker()
{
    pthread_t thread_id;
    struct worker_info* info = (struct worker_info*) malloc(sizeof(struct worker_info));
    pthread_create(&thread_id, NULL, start_render_worker, info);
    info->thread_id = thread_id;
    render_workers[render_worker_count] = info;
    render_worker_count++;
}

void add_save_worker()
{
    pthread_t thread_id;
    struct worker_info* info = (struct worker_info*) malloc(sizeof(struct worker_info));
    pthread_create(&thread_id, NULL, start_save_worker, info);
    info->thread_id = thread_id;
    save_workers[save_worker_count] = info;
    save_worker_count++;
}

// Post queue
#define MAIN_THREAD_QUEUE_SIZE 64
struct main_thread_queue work_queue;

// Public
// Enques work to work quueue - 
void main_thread_post(struct main_thread_work work)
{
    push_work_queue(&work_queue, work);
}

// Called by main thread
void push_download_queue(struct canvas_info result)
{
    download_stack[download_stack_top] = result;
    download_stack_top++;
}

// Called by download worker
void push_render_queue(struct downloaded_result result)
{
    render_stack[render_stack_top] = result;
    render_stack_top++;
}

// Called by render worker
void push_save_queue(struct render_result result)
{
    save_stack[download_stack_top] = result;
    save_stack_top++;
}

// Start all workers, initiate rendering backups
void start_generation()
{
    char* hash = malloc(40);

    // Setup backups dir variable
    getcwd(backups_dir, sizeof(backups_dir));
    strcat(backups_dir, "/backups/");
    mkdir(backups_dir, 0755);

    log_message("Using commit hashes file, see --help for info on more commands.\n");
    // Read list of all
    FILE* file = fopen("commit_hashes.txt", "r");
    if (file == NULL) {
        fprintf(stderr, "\x1b[1;31mError, could not locate commit hashes file (commit_hashes.txt)\n");
        exit(EXIT_FAILURE);
    }

    long file_lines = 7957; //flines(file);
    char* lines = malloc(file_lines * 40);

    for (int i = 0; i < file_lines; i++) {
        char* line = NULL;
        unsigned long length = 0; // This should always be 40 anyway
        getline(&line, &length, file);
        memcpy(lines + (i * 40), line, 40);
    }

    for (int i = 0; i < DEFAULT_DOWNLOAD_WORKER_COUNT; i++)
    {
        add_download_worker();
    }
    for (int i = 0; i < DEFAULT_SAVE_WORKER_COUNT; i++)
    {
        add_render_worker();
    }
    for (int i = 0; i < DEFAULT_SAVE_WORKER_COUNT; i++)
    {
        add_save_worker();
    }
}

// Stop the entire program, will cleanup all resources
void stop_generation()
{
    // foreach download worker -> curl_easy_cleanup(curl_handle), etc

    cleanup_work_queue(&work_queue);
    curl_global_cleanup();
    pthread_exit(NULL);
    exit(0);
}

void start_main_thread()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    init_work_queue(&work_queue, MAIN_THREAD_QUEUE_SIZE);
    pthread_mutex_init(&work_wait_mutex, NULL);

    pthread_mutex_lock(&work_wait_mutex);
    while (1)
    {
        // Will wait for work to arrive via work queue, at which point
        // it will unlock and main thread will pop & process work
        pthread_mutex_lock(&work_wait_mutex);
        struct main_thread_work work = pop_work_queue(&work_queue);
        work.func(work.data);
    }
    
}
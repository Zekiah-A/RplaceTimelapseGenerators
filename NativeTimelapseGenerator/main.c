#include <stdio.h>
#include <png.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

struct colour {
    char red;
    char green;
    char blue;
};

char colours[32][3] = {
    { 109, 0, 26 },
    { 190, 0, 57 },
    { 255, 69, 0 },
    { 255, 168, 0 },
    { 255, 214, 53 },
    { 255, 248, 184 },
    { 0, 163, 104 },
    { 0, 204, 120 },
    { 126, 237, 86 },
    { 0, 117, 111 },
    { 0, 158, 170 },
    { 0, 204, 192 },
    { 36, 80, 164 },
    { 54, 144, 234 },
    { 81, 233, 244 },
    { 73, 58, 193 },
    { 106, 92, 255 },
    { 148, 179, 255 },
    { 129, 30, 159 },
    { 180, 74, 192 },
    { 228, 171, 255 },
    { 222, 16, 127 },
    { 255, 56, 129 },
    { 255, 153, 170 },
    { 109, 72, 47 },
    { 156, 105, 38 },
    { 255, 180, 112 },
    { 0, 0, 0 },
    { 81, 82, 82 },
    { 137, 141, 144 },
    { 212, 215, 217 },
    { 255, 255, 255 }
};

int width = 2000;
int height = 2000;
int backups_finished = 0;
char backups_dir[256];
pthread_mutex_t fetch_lock;

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

static size_t write_fetch(void* contents, size_t size, size_t nmemb, void* userp) {
    // Size * number of elements
    size_t data_size = size * nmemb;
    struct memory_fetch* fetch = (struct memory_fetch*) userp;

    char* new_memory = realloc(fetch->memory, fetch->size + data_size + 1);
    if (new_memory == NULL) {
        printf("Out of memory, can not carray out fetch reallocation\n");
        return 0;
    }

    fetch->memory = new_memory;
    memcpy(&(fetch->memory[fetch->size]), contents, data_size);
    fetch->size += data_size;
    fetch->memory[fetch->size] = 0;
 
    return data_size;
}


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

        char backup_name[256];
        snprintf(backup_name, 256, "%sbackup_%s.png", backups_dir, line);

        FILE *file = fopen(backup_name, "wb");
        if (!file) {
            printf("Error: could not open file for writing.\n");
            continue;
        }

        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) {
            printf("Error: could not create PNG write structure.\n");
            fclose(file);
            continue;
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            printf("Error: could not create PNG info structure.\n");
            png_destroy_write_struct(&png_ptr, NULL);
            fclose(file);
            continue;
        }

        png_init_io(png_ptr, file);
        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png_ptr, info_ptr);

        png_bytep row_pointers[height];
        
        // Okay now we can actually draw
        for (int i = 0; i < height; i++) {
            row_pointers[i] = (png_bytep) calloc(3 * width, sizeof(png_byte));
            for (int j = 0; j < width; j++) {
                int index = i * width + j;
                memcpy(&row_pointers[i][3 * j], colours[chunk.memory[index]], 3);
            }
        }

        png_write_image(png_ptr, row_pointers);

        for (int i = 0; i < height; i++) {
            free(row_pointers[i]);
        }

        png_write_end(png_ptr, NULL);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(file);

        printf("Completed backup %s, finished %d.\n", line, backups_finished);
        backups_finished++;
    }

    threads_finished++;
    free(chunk.memory);
    curl_easy_cleanup(curl);
    return NULL;
}

int main(char* argv) {
    // Download commit hashes file
    // TODO: see file top


    // Read list of all
    FILE* file = fopen("commit_hashes.txt", "r");
    if (file == NULL) {
        printf("Very big error, could not locate commit hashes file\n");
        return 1;
    }

    // Setup backups dir variable
    getcwd(backups_dir, sizeof(backups_dir));
    strcat(backups_dir, "/backups/");
    mkdir(backups_dir, 0755);

    long file_lines = 7957; //flines(file);
    char* lines = malloc(file_lines * 40);

    for (int i = 0; i < file_lines; i++) {
        char* line = NULL;
        unsigned long length = 0; // This should always be 40 anyway
        getline(&line, &length, file);
        memcpy(lines + (i * 40), line, 40);
    }

    if (pthread_mutex_init(&fetch_lock, NULL) != 0)
    {
        printf("Fetch lock mutex init failed\n");
        return 1;
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

    pthread_mutex_destroy(&fetch_lock);
    return 0;
}

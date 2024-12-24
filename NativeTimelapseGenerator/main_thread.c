#include <git2/commit.h>
#include <signal.h>
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
#include "canvas_downloader.h"
#include "image_generator.h"
#include "main_thread.h"
#include "canvas_saver.h"
#include "worker_structs.h"
#include <stdbool.h>
#include <execinfo.h>
#include <git2.h>
#include <errno.h>
#include <sqlite3.h>

// Reads canvas list, manages jobs for workers
#define LOG_HEADER "[main thread] "

// SHARED BETWEEN-WORKER MEMORY
#define STACK_SIZE_MAX 256
#define DEFAULT_DOWNLOAD_WORKER_COUNT 4
Stack download_stack;
#define DEFAULT_RENDER_WORKER_COUNT 2
Stack render_stack;
#define DEFAULT_SAVE_WORKER_COUNT 1
Stack save_stack;

// WORKER THREADS
#define WORKER_MAX 100
struct worker_info* download_workers[WORKER_MAX] = { };
int download_worker_top = -1;
struct worker_info* render_workers[WORKER_MAX] = { };
int render_worker_top = -1;
struct worker_info* save_workers[WORKER_MAX] = { };
int save_worker_top = -1;

time_t completed_backups_date = 0;
int completed_backups_since = 0;
int completed_backups = 0;
CanvasInfo* completed_canvas_info = NULL;

bool work_queue_replenished = false;
pthread_mutex_t download_pop_mutex;
bool download_stack_replenished = false;
pthread_mutex_t render_pop_mutex;
bool render_stack_replenished = false;
pthread_mutex_t save_pop_mutex;
bool save_stack_replenished = false;

const char* _download_base_url;

// DATABASE
sqlite3* database = NULL;

// Private
void init_work_queue(MainThreadQueue* queue, size_t capacity)
{
	queue->work = (MainThreadWork*) malloc(sizeof(MainThreadWork) * capacity);
	if (!queue->work) {
		stop_console();
		fprintf(stderr, "Failed to initialise main thread work queue\n");
		exit(EXIT_FAILURE);
	}

	queue->capacity = capacity;
	queue->front = 0;
	queue->rear = 0;
	pthread_mutex_init(&queue->mutex, NULL);
}

// Dequeue a message
void push_work_queue(MainThreadQueue* queue, MainThreadWork work)
{
	pthread_mutex_lock(&queue->mutex);

	// Check for queue overflow
	if ((queue->rear + 1) % queue->capacity == queue->front) {
		stop_console();
		fprintf(stderr, "Error - Work queue overflow.\n");
		pthread_mutex_unlock(&queue->mutex);
		exit(EXIT_FAILURE);
	}

	// Enqueue the message
	queue->work[queue->rear] = work;
	queue->rear = (queue->rear + 1) % queue->capacity;
	
	work_queue_replenished = true;
	pthread_mutex_unlock(&queue->mutex);
}

struct main_thread_work pop_work_queue(MainThreadQueue* queue)
{
	pthread_mutex_lock(&queue->mutex);

	// Check underflow
	if (queue->front == queue->rear) {
		stop_console();
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

int flines(FILE* file)
{
	fseek(file, 0, SEEK_SET);
	char ch = 0;
	int count = 0;
	while ((ch = getc(file)) != EOF) {
		if (ch == '\n') {
			count++;
		}
	}
	fseek(file, 0, SEEK_SET);
	return count;
}

void add_download_worker()
{
	pthread_t thread_id;
	WorkerInfo* info = (struct worker_info*) malloc(sizeof(WorkerInfo));
	info->worker_id = ++download_worker_top;
	pthread_create(&thread_id, NULL, start_download_worker, info);
	info->thread_id = thread_id;
	download_workers[download_worker_top] = info;
	update_worker_stats(WORKER_STEP_DOWNLOAD, download_worker_top + 1);
}

void remove_download_worker()
{
	if (download_worker_top < 0) {
		stop_console();
		fprintf(stderr, "Error - download worker underflow occurred\n");
		exit(EXIT_FAILURE);
	}
	WorkerInfo* info = download_workers[download_worker_top];
	pthread_cancel(info->thread_id);
	download_worker_top--;
	free(info->download_worker_data);
	free(info);
	update_worker_stats(WORKER_STEP_DOWNLOAD, download_worker_top + 1);
}

void add_render_worker()
{
	pthread_t thread_id;
	WorkerInfo* info = (WorkerInfo*) malloc(sizeof(WorkerInfo));
	info->worker_id = ++render_worker_top;
	pthread_create(&thread_id, NULL, start_render_worker, info);
	info->thread_id = thread_id;
	render_workers[render_worker_top] = info;
	update_worker_stats(WORKER_STEP_RENDER, render_worker_top + 1);
}

void remove_render_worker()
{
	if (render_worker_top < 0) {
		stop_console();
		fprintf(stderr, "Error - render worker underflow occurred\n");
		exit(EXIT_FAILURE);
	}
	WorkerInfo* info = render_workers[render_worker_top];
	pthread_cancel(info->thread_id);
	render_worker_top--;
	free(info->render_worker_data);
	free(info);
	update_worker_stats(WORKER_STEP_RENDER, render_worker_top + 1);
}

void add_save_worker()
{
	pthread_t thread_id;
	WorkerInfo* info = (WorkerInfo*) malloc(sizeof(WorkerInfo));
	info->worker_id = ++save_worker_top;
	pthread_create(&thread_id, NULL, start_save_worker, info);
	info->thread_id = thread_id;
	save_workers[save_worker_top] = info;
	update_worker_stats(WORKER_STEP_SAVE, save_worker_top + 1);
}

void remove_save_worker()
{
	if (save_worker_top < 0) {
		stop_console();
		fprintf(stderr, "Error - save worker underflow occurred\n");
		exit(EXIT_FAILURE);
	}
	struct worker_info* info = save_workers[save_worker_top];
	pthread_cancel(info->thread_id);
	save_worker_top--;
	free(info->save_worker_data);
	free(info);
	update_worker_stats(WORKER_STEP_SAVE, save_worker_top + 1);
}

// Post queue
#define MAIN_THREAD_QUEUE_SIZE 64
MainThreadQueue work_queue = { };

// Public
// Enques work to work queue - 
void main_thread_post(MainThreadWork work)
{
	push_work_queue(&work_queue, work);
}

// Called by main thread
void push_download_stack(CanvasInfo result)
{
	push_stack(&download_stack, &result);
}

// Called by download worker
void push_render_stack(DownloadedResult result)
{
	push_stack(&render_stack, &result);
}

// Called by render worker
void push_save_stack(RenderResult result)
{
	push_stack(&save_stack, &result);
}

// Called by save worker
void push_completed_frame(CanvasInfo info)
{
	CanvasInfo* heap_canvas_info = (CanvasInfo*) malloc(sizeof(CanvasInfo));
	memcpy(heap_canvas_info, &info, sizeof(CanvasInfo));

	// Remove previous heap completed canvas info, finally frees the strings that both stack/heap versions of this uses
	if (completed_canvas_info != NULL) {
		// TODO: Investigate segfault
		/*free(completed_canvas_info->url);
		free(completed_canvas_info->commit_hash);
		free(completed_canvas_info->save_path);
		free(completed_canvas_info);*/
	}

	completed_backups++;
	completed_backups_since++;
	completed_canvas_info = heap_canvas_info;
}

// Called by save worker on main thread, will forward collected info to UI thread
void collect_backup_stats()
{
	time_t current_time = time(0);
	float backups_per_second = ((float)(current_time - completed_backups_date)) / (float) completed_backups_since;
	update_backups_stats(completed_backups, backups_per_second, *completed_canvas_info);
	completed_backups_since = 0;
	completed_backups_date = current_time;
}

// Forward declarations
void* read_commit_hashes(FILE* file);
FILE* commit_hashes_stream = NULL;

// Called by download worker
CanvasInfo pop_download_stack(int worker_id)
{
	CanvasInfo result;
	while (pop_stack(&download_stack, &result) == 1) {
		usleep(10000); // Wait for 10ms
	}
	return result;
}

// Called by render worker
DownloadedResult pop_render_stack(int worker_id)
{
	DownloadedResult result;
	while (pop_stack(&render_stack, &result) == 1) {
		usleep(10000); // Wait for 10ms
	}
	return result;
}

// Called by save worker
RenderResult pop_save_stack(int worker_id)
{
	RenderResult result;
	while (pop_stack(&save_stack, &result) == 1) {
		usleep(10000); // Wait for 10ms
	}
	return result;
}

#define MAX_HASHES_LINE_LEN 256
#define EXPECT_COMMIT_LINE 0
#define EXPECT_AUTHOR_LINE 1
#define EXPECT_DATE_LINE 2
const char* expects[] = { "Commit", "Author", "Date" };

// Commit: ...\n, Author: ...\n, Date: ...\n, top is most recent, bottom of log is longest ago
//  - Both files have been manually assimilated into commit_hashes.txt
//  - canvas_1_commit_hashes.txt - only process commits from "root", from after 1672531200,
//  - commit (1672531012, 48a24088916a63319a56c245a0290d66cf29e076) is the first commit (Jan 1st), 2023
//  - repo_commit_hashes.txt - Only process commits from "nebulus"
// Fetch methods
// - https://raw.githubusercontent.com/rslashplace2/rslashplace2.github.io/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place
// - https://github.com/rslashplace2/rslashplace2.github.io/blob/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place?raw=true
// - https://github.com/rslashplace2/rslashplace2.github.io/raw/82153c44c6a3cd3f248c3cd20f1ce6b7f0ce4b1e/place
// Example:
// Commit: 49fdda2c4f38f7afd8af2c178b450e8f0fcae65b
// Author: nebulus
// Date: 1704298864
// STRICT: Called by main thread
void* read_commit_hashes(FILE* file)
{
	int expect = EXPECT_COMMIT_LINE;
	long file_lines = flines(file);
	CanvasInfo new_canvas_info = {};
	char line[MAX_HASHES_LINE_LEN];
	char* result = NULL;
	int line_index = 0;
	while ((result = fgets(line, MAX_HASHES_LINE_LEN, file)) != NULL) {
		line_index++;
		int result_len = strlen(result); // strip \n
		result[--result_len] = '\0';

		// Comment or ignore
		if (result_len == 0 || result[0] == '#' || result[0] == '\n') {
			log_message("(Line %d) Ignoring comment '%s'", line_index, result);
			continue;
		}

		if (strncmp(result, "Commit: ", 8) == 0) {
			if (expect != EXPECT_COMMIT_LINE) {
				log_message("(Line %d:%s) expected %s property, skipping", line_index, line, expects[expect]);
				continue;
			}
			
			int hash_len = result_len - 8;
			char* commit_hash = malloc(hash_len + 1);
			strcpy(commit_hash, result + 8);
			new_canvas_info.commit_hash = commit_hash;            
			expect = EXPECT_AUTHOR_LINE;
		}
		else if (strncmp(result, "Author: ", 8) == 0) {
			if (expect != EXPECT_AUTHOR_LINE) {
				log_message("(Line %d:%s) expected %s property, skipping", line_index, line, expects[expect]);
				continue;
			}

			char* author = result + 8;
			// HACK: Use author to determine the repo URL of the backup
			if (strcmp(author, "root") != 0 && strcmp(author, "nebulus") != 0) {
				log_message("(Line %d:%s) commit is not from server push. Ignoring", line_index, line);
				// Ignore this commit, it is not a canvas push
				expect = EXPECT_COMMIT_LINE;
				memset(&new_canvas_info, 0, sizeof(CanvasInfo)); // Wipe for reuse
				continue;
			}

			expect = EXPECT_DATE_LINE;
		}
		else if (strncmp(result, "Date: ", 6) == 0) {
			if (expect != EXPECT_DATE_LINE) {
				log_message("(Line %d:%s) expected %s property, skipping", line_index, line, expects[expect]);
				continue;
			}

			int date_len = strlen(result) - 6;
			char* date = malloc(date_len + 1);
			strcpy(date, result + 6);
			time_t date_int = strtoll(date, NULL, 10);
			new_canvas_info.date = date_int;

			push_download_stack(new_canvas_info);
			memset(&new_canvas_info, 0, sizeof(CanvasInfo)); // Wipe for reuse

			if (download_stack.top >= STACK_SIZE_MAX - 4) { // HACK: -4 For some reaason it still overflows and adds too many. Concurrency bug?
				// We will come back later
				log_message(LOG_HEADER"Bufferred %d commit records into download stack. Pausing until needs replenish", download_stack.top + 1);
				break;
			}

			expect = EXPECT_COMMIT_LINE;
		}
		else {
			stop_console();
			fprintf(stderr, "(Line %d:%s) Failed to read commit hashes, invalid character\n", line_index, line);
			exit(EXIT_FAILURE);
		}
	}
	if (download_stack.top < 0) {
		stop_console();
		fprintf(stderr, "Could not find any unprocessed backups from commit_hashes.txt\n");
		exit(EXIT_FAILURE);
	}
	return NULL;
}

int last_progress_percentage = -1;

int progress_callback(const git_indexer_progress* stats, void* payload)
{
	int total_objects = stats->total_objects;
	int received_objects = stats->received_objects;
	int indexed_objects = stats->indexed_objects;
	int percentage = (total_objects > 0) ? (received_objects * 100 / total_objects) : 0;
	if (percentage != last_progress_percentage) {
		log_message(LOG_HEADER"Cloning progress: %d%% (%d/%d) - Indexed: %d", percentage, received_objects, total_objects, indexed_objects);
	}
	last_progress_percentage = percentage;
	return 0;
}

bool is_repo_cloned(const char* repo_path)
{
	git_repository* repo = NULL;
	int result = git_repository_open(&repo, repo_path);
	if (result == 0) {
		git_repository_free(repo);
		return true;
	}
	return false;
}

bool is_commit_hashes_up_to_date(const char* log_file_name)
{
	FILE* file = fopen(log_file_name, "r");
	if (!file) {
		return false;
	}
	fclose(file);
	return true;
}

void append_new_commits_to_log(git_repository* repo, const char* log_file_name)
{
	git_revwalk* walker = NULL;
	if (git_revwalk_new(&walker, repo) != 0) {
		const git_error* e = git_error_last();
		fprintf(stderr, "Error creating revwalk: %s\n", e->message);
		return;
	}

	git_revwalk_sorting(walker, GIT_SORT_TIME);
	git_revwalk_push_head(walker);

	FILE* log_file = fopen(log_file_name, "a");
	if (!log_file) {
		fprintf(stderr, "Error opening log file for appending\n");
		git_revwalk_free(walker);
		return;
	}

	git_oid oid;
	int commit_count = 0;
	while (git_revwalk_next(&oid, walker) == 0) {
		git_commit* commit = NULL;
		if (git_commit_lookup(&commit, repo, &oid) != 0) {
			const git_error* e = git_error_last();
			fprintf(stderr, "Error looking up commit: %s\n", e->message);
			continue;
		}

		const char* commit_hash = git_oid_tostr_s(&oid);
		const char* author_name = git_commit_author(commit)->name;
		git_time_t commit_time = git_commit_time(commit);
		char commit_date[20];
		snprintf(commit_date, sizeof(commit_date), "%ld", commit_time);

		fprintf(log_file, "Commit: %s\nAuthor: %s\nDate: %s\n", commit_hash, author_name, commit_date);
		commit_count++;

		if (commit_count % 100 == 0) {
			log_message(LOG_HEADER"Appended %d new commits...", commit_count);
		}

		git_commit_free(commit);
	}

	fclose(log_file);
	git_revwalk_free(walker);
	log_message(LOG_HEADER"Appended new commits to log file %s", log_file_name);
}

const char* get_repo_commit_hashes(const char* repo_url)
{
	log_message(LOG_HEADER"Initializing libgit2...");
	git_libgit2_init();

	const char* repo_path = "./repo";
	const char* log_file_name = "repo_commit_hashes.txt";

	if (is_repo_cloned(repo_path)) {
		log_message(LOG_HEADER"Repository already cloned.");

		if (is_commit_hashes_up_to_date(log_file_name)) {
			log_message(LOG_HEADER"Commit hashes log is up to date.");
			return log_file_name;
		}
		else {
			log_message(LOG_HEADER"Commit hashes log is outdated. Appending new commits...");
			git_repository* repo = NULL;
			if (git_repository_open(&repo, repo_path) != 0) {
				const git_error* e = git_error_last();
				fprintf(stderr, "Error opening repository: %s\n", e->message);
				git_libgit2_shutdown();
				return NULL;
			}
			append_new_commits_to_log(repo, log_file_name);
			git_repository_free(repo);
			return log_file_name;
		}
	}

	log_message(LOG_HEADER"Cloning repository from %s...", repo_url);

	git_repository* repo = NULL;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
	callbacks.transfer_progress = progress_callback;
	clone_opts.fetch_opts.callbacks = callbacks;

	log_message(LOG_HEADER"Starting repository clone...");
	if (git_clone(&repo, repo_url, repo_path, &clone_opts) != 0) {
		const git_error* e = git_error_last();
		fprintf(stderr, "Error cloning repository: %s\n", e->message);
		git_libgit2_shutdown();
		return NULL;
	}
	log_message(LOG_HEADER"Repository cloned successfully.");

	log_message(LOG_HEADER"Creating revwalk...");
	git_revwalk* walker = NULL;
	if (git_revwalk_new(&walker, repo) != 0) {
		const git_error* e = git_error_last();
		fprintf(stderr, "Error creating revwalk: %s\n", e->message);
		git_repository_free(repo);
		git_libgit2_shutdown();
		return NULL;
	}
	log_message(LOG_HEADER"Revwalk created successfully.");

	git_revwalk_sorting(walker, GIT_SORT_TIME);
	git_revwalk_push_head(walker);

	FILE* log_file = fopen(log_file_name, "w");
	if (!log_file) {
		fprintf(stderr, "Error opening log file for writing\n");
		git_revwalk_free(walker);
		git_repository_free(repo);
		git_libgit2_shutdown();
		return NULL;
	}
	log_message(LOG_HEADER"Log file %s created successfully.", log_file_name);

	git_oid oid;
	int commit_count = 0;
	while (git_revwalk_next(&oid, walker) == 0) {
		git_commit* commit = NULL;
		if (git_commit_lookup(&commit, repo, &oid) != 0) {
			const git_error* e = git_error_last();
			fprintf(stderr, "Error looking up commit: %s\n", e->message);
			continue;
		}

		const char* commit_hash = git_oid_tostr_s(&oid);
		const char* author_name = git_commit_author(commit)->name;
		git_time_t commit_time = git_commit_time(commit);
		char commit_date[20];
		snprintf(commit_date, sizeof(commit_date), "%ld", commit_time);

		fprintf(log_file, "Commit: %s\nAuthor: %s\nDate: %s\n", commit_hash, author_name, commit_date);
		commit_count++;

		if (commit_count % 100 == 0) {
			log_message(LOG_HEADER"Processed %d commits...", commit_count);
		}

		git_commit_free(commit);
	}

	fclose(log_file);
	git_revwalk_free(walker);
	git_repository_free(repo);
	git_libgit2_shutdown();
	log_message(LOG_HEADER"Repository clone and log complete. Log saved to %s", log_file_name);

	char* result = strdup(log_file_name);
	return result;
}

int database_create_callback(void *data, int argc, char **argv, char **col_name)
{
	for (int i = 0; i < argc; i++) {
		log_message(LOG_HEADER"%s = %s\n", col_name[i], argv[i] ? argv[i] : "NULL");
	}
	return 0;
}

bool try_create_database()
{
	char* err_msg = NULL;
	const char* schema_file = "schema.sql";

	// Open database connection
	int result = sqlite3_open("canvas_database.db", &database);
	if (result != SQLITE_OK) {
		log_message(LOG_HEADER"Cannot open database: %s\n", sqlite3_errmsg(database));
		return false;
	}

	// Read schema file
	FILE* file = fopen(schema_file, "r");
	if (!file) {
		log_message(LOG_HEADER"Cannot open schema file: %s\n", schema_file);
		sqlite3_close(database);
		return false;
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	rewind(file);

	// Allocate memory to hold the schema content
	char* sql = (char*)malloc(file_size + 1);
	if (sql == NULL) {
		log_message(LOG_HEADER"Memory allocation failed\n");
		fclose(file);
		sqlite3_close(database);
		return false;
	}

	// Read the schema from the file
	fread(sql, 1, file_size, file);
	sql[file_size] = '\0';
	fclose(file);

	// Execute schema SQL statements
	result = sqlite3_exec(database, sql, database_create_callback, 0, &err_msg);
	if (result != SQLITE_OK) {
		log_message(LOG_HEADER"Failed to create database: %s\n", err_msg);
		sqlite3_free(err_msg);
		free(sql);
		sqlite3_close(database);
		return false;
	}

	free(sql);
	return true;
}

const char* get_download_base_url()
{
	return _download_base_url;
}

// Start all workers, initiate rendering backups
void start_generation(const char* download_base_url, const char* repo_url, const char* log_file_name)
{
	_download_base_url = download_base_url;

	if (!try_create_database()) {
		stop_console();
		log_message(LOG_HEADER"Error creating database\n");
		exit(EXIT_FAILURE);
	}

	// Either source commit info from commit hashes or repo URL
	log_message(LOG_HEADER"Starting generation process...");
	if (repo_url) {
		log_file_name = get_repo_commit_hashes(repo_url);
	}
	if (!log_file_name) {
		stop_console();
		log_message(LOG_HEADER"Error, could not locate commit hashes file\n");
		exit(EXIT_FAILURE);
	}

	log_message(LOG_HEADER"Opening log file %s for reading...", log_file_name);
	FILE* file = fopen(log_file_name, "r");
	if (file == NULL) {
		stop_console();
		log_message(LOG_HEADER"Error, could not locate commit hashes file (%s)\n", log_file_name);
		exit(EXIT_FAILURE);
	}
	commit_hashes_stream = file;

	long file_lines = flines(file);
	log_message(LOG_HEADER"Detected %d lines in %s", file_lines, log_file_name);
	read_commit_hashes(file);

	// Create required directories
	if (mkdir("backups", 0777) == -1) {
		if (errno != EEXIST) {
			stop_console();
			log_message(LOG_HEADER, "Error creating backups directory\n");
			exit(EXIT_FAILURE);
		}
	}
	if (mkdir("dates", 0777) == -1) {
		if (errno != EEXIST) {
			stop_console();
			log_message(LOG_HEADER, "Error creating dates directory\n");
			exit(EXIT_FAILURE);
		}
	}

	log_message(LOG_HEADER"Starting backup generation...");
	for (int i = 0; i < DEFAULT_DOWNLOAD_WORKER_COUNT; i++) {
		log_message(LOG_HEADER"Adding download worker %d...", i + 1);
		add_download_worker();
	}
	for (int i = 0; i < DEFAULT_RENDER_WORKER_COUNT; i++) {
		log_message(LOG_HEADER"Adding render worker %d...", i + 1);
		add_render_worker();
	}
	for (int i = 0; i < DEFAULT_SAVE_WORKER_COUNT; i++) {
		log_message(LOG_HEADER"Adding save worker %d...", i + 1);
		add_save_worker();
	}
	log_message(LOG_HEADER"Backup generation started.");
}

// Often called by UI. Cleanly shutdown generation side of program, will cleanup all resources
void stop_generation()
{
	// Cleanup work queue
	free(work_queue.work);
	pthread_mutex_destroy(&work_queue.mutex);

	// Cleanup stacks
	free_stack(&download_stack);
	free_stack(&render_stack);
	free_stack(&save_stack);

	// Terminate and cleanup all workers
	terminate_and_cleanup_workers(download_workers, &download_worker_top, DOWNLOAD_WORKER_TYPE);
	terminate_and_cleanup_workers(render_workers, &render_worker_top, RENDER_WORKER_TYPE);
	terminate_and_cleanup_workers(save_workers, &save_worker_top, SAVE_WORKER_TYPE);

	// Cleanup globals
	curl_global_cleanup();
	pthread_exit(NULL);
	exit(0);
}

void safe_segfault_exit(int sig_num)
{
	void* array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig_num);
	backtrace_symbols_fd(array, size, STDERR_FILENO);

	stop_console();
	fprintf(stderr, "Fatal - Segmentation fault\n");

	exit(EXIT_FAILURE);
}

void start_main_thread(bool start, const char* download_base_url, const char* repo_url, const char* log_file_name)
{
	signal(SIGSEGV, safe_segfault_exit);
	curl_global_init(CURL_GLOBAL_DEFAULT);
	completed_backups_date = time(0);

	init_work_queue(&work_queue, MAIN_THREAD_QUEUE_SIZE);
	init_stack(&download_stack, sizeof(CanvasInfo), STACK_SIZE_MAX);
	init_stack(&render_stack, sizeof(DownloadedResult), STACK_SIZE_MAX);
	init_stack(&save_stack, sizeof(RenderResult), STACK_SIZE_MAX);

	if (start) {
		start_generation(download_base_url, repo_url, log_file_name);
	}
	while (true) {
		// Will wait for work to arrive via work queue, at which point
		// main thread will pop & process work
		while (!work_queue_replenished) {
			usleep(10000); // Wait for 10ms
		}
		work_queue_replenished = false;

		struct main_thread_work work = pop_work_queue(&work_queue);

		switch (work.arg_count) {
			case 0:
				((void (*)(void))work.func)();
				break;
			case 1:
				((void (*)(void*))work.func)(work.args.args[0]);
				break;
			case 2:
				((void (*)(void*, void*))work.func)(work.args.args[0], work.args.args[1]);
				break;
			case 3:
				((void (*)(void*, void*, void*))work.func)(work.args.args[0], work.args.args[1], work.args.args[2]);
				break;
			case 4:
				((void (*)(void*, void*, void*, void*))work.func)(work.args.args[0], work.args.args[1], work.args.args[2], work.args.args[3]);
				break;
			case 5:
				((void (*)(void*, void*, void*, void*, void*))work.func)(work.args.args[0], work.args.args[1], work.args.args[2], work.args.args[3], work.args.args[4]);
				break;
			case 6:
				((void (*)(void*, void*, void*, void*, void*, void*))work.func)(work.args.args[0], work.args.args[1], work.args.args[2], work.args.args[3], work.args.args[4], work.args.args[5]);
				break;
			case 7:
				((void (*)(void*, void*, void*, void*, void*, void*, void*))work.func)(work.args.args[0], work.args.args[1], work.args.args[2], work.args.args[3], work.args.args[4], work.args.args[5], work.args.args[6]);
				break;
			case 8:
				((void (*)(void*, void*, void*, void*, void*, void*, void*, void*))work.func)(work.args.args[0], work.args.args[1], work.args.args[2], work.args.args[3], work.args.args[4], work.args.args[5], work.args.args[6], work.args.args[7]);
				break;
			default:
				stop_console();
				fprintf(stderr, "Error - Unsupported number of arguments: %d\n", work.arg_count);
				exit(EXIT_FAILURE);
		}
	}
}

void init_stack(Stack* stack, size_t item_size, int max_size)
{
	stack->items = malloc(item_size * max_size);
	stack->item_size = item_size;
	stack->top = -1;
	stack->max_size = max_size;
	pthread_mutex_init(&stack->mutex, NULL);
	stack->replenished = false;
}

void push_stack(Stack* stack, void* item)
{
	pthread_mutex_lock(&stack->mutex);

	if (stack->top >= stack->max_size - 1) {
		stop_console();
		fprintf(stderr, "Error - stack overflow occurred\n");
		pthread_mutex_unlock(&stack->mutex);
		exit(EXIT_FAILURE);
	}

	stack->top++;
	memcpy((char*)stack->items + stack->top * stack->item_size, item, stack->item_size);
	stack->replenished = true;

	pthread_mutex_unlock(&stack->mutex);
}

int pop_stack(Stack* stack, void* item)
{
	pthread_mutex_lock(&stack->mutex);

	if (stack->top < 0) {
		memset(item, 0, stack->item_size);
		pthread_mutex_unlock(&stack->mutex);
		return 1;
	}

	memcpy(item, (char*)stack->items + stack->top * stack->item_size, stack->item_size);
	stack->top--;

	pthread_mutex_unlock(&stack->mutex);
	return 0;
}

void free_stack(Stack* stack)
{
	free(stack->items);
	pthread_mutex_destroy(&stack->mutex);
}

void terminate_and_cleanup_workers(WorkerInfo** workers, int* worker_top, WorkerType worker_type)
{
	for (int i = 0; i <= *worker_top; i++) {
		if (worker_type == DOWNLOAD_WORKER_TYPE) {
			curl_easy_cleanup(workers[i]->download_worker_data->curl_handle);
			free(workers[i]->download_worker_data);
		}
		else if (worker_type == RENDER_WORKER_TYPE) {
			free(workers[i]->render_worker_data);
		}
		else if (worker_type == SAVE_WORKER_TYPE) {
			free(workers[i]->save_worker_data);
		}
		pthread_cancel(workers[i]->thread_id);
		free(workers[i]);
	}
	*worker_top = -1;
}

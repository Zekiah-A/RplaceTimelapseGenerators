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
#include <stdbool.h>
#include <execinfo.h>
#include <git2.h>
#include <git2/commit.h>
#include <errno.h>
#include <sqlite3.h>
#include <avcall.h>

#include "console.h"
#include "main_thread.h"
#include "memory_utils.h"
#define STB_DS_IMPLEMENTATION
#include "lib/stb/stb_ds.h"

#include "workers/worker_structs.h"
#include "workers/download_worker.h"
#include "workers/render_worker.h"
#include "workers/save_worker.h"

// Reads canvas list, manages jobs for workers
#define LOG_HEADER "[main thread] "

// SHARED BETWEEN-WORKER MEMORY
#define STACK_SIZE_MAX 256
Stack download_stack;
Stack render_stack;
Stack save_stack;

bool work_queue_replenished = false;
pthread_mutex_t download_pop_mutex;
bool download_stack_replenished = false;
pthread_mutex_t render_pop_mutex;
bool render_stack_replenished = false;
pthread_mutex_t save_pop_mutex;
bool save_stack_replenished = false;

// WORKER THREADS
#define DEFAULT_DOWNLOAD_WORKER_COUNT 4
#define DEFAULT_RENDER_WORKER_COUNT 2
#define DEFAULT_SAVE_WORKER_COUNT 1
WorkerInfo** download_workers = NULL; // stb array
WorkerInfo** render_workers = NULL; // stb array
WorkerInfo** save_workers = NULL; // stb array

time_t completed_backups_date = 0;
int completed_backups_since = 0;
int completed_backups = 0;
CanvasInfo* completed_canvas_info = NULL;

// DATABASE
sqlite3* _database = NULL;

// CONFIG
Config _config = { 0 };

// WORKER DATAS
DownloadWorkerShared _download_worker_shared = {
	.user_map = NULL
};
RenderWorkerShared _render_worker_shared;
SaveWorkerShared _save_worker_shared;

// Private
void init_work_queue(MainThreadQueue* queue, size_t capacity)
{
	queue->work = (av_alist*) malloc(sizeof(av_alist) * capacity);
	if (!queue->work) {
		stop_console();
		log_message(LOG_ERROR, "Failed to initialise main thread work queue\n");
		exit(EXIT_FAILURE);
	}

	queue->capacity = capacity;
	queue->front = 0;
	queue->rear = 0;
	pthread_mutex_init(&queue->mutex, NULL);
}

// Dequeue a message
void push_work_queue(MainThreadQueue* queue, av_alist work)
{
	pthread_mutex_lock(&queue->mutex);

	// Check for queue overflow
	if ((queue->rear + 1) % queue->capacity == queue->front) {
		stop_console();
		log_message(LOG_ERROR, "Error - Work queue overflow.\n");
		pthread_mutex_unlock(&queue->mutex);
		exit(EXIT_FAILURE);
	}

	// Enqueue the message
	queue->work[queue->rear] = work;
	queue->rear = (queue->rear + 1) % queue->capacity;
	
	work_queue_replenished = true;
	pthread_mutex_unlock(&queue->mutex);
}

av_alist pop_work_queue(MainThreadQueue* queue)
{
	pthread_mutex_lock(&queue->mutex);

	// Check underflow
	if (queue->front == queue->rear) {
		stop_console();
		log_message(LOG_ERROR, "Error - Work queue underflow.\n");
		pthread_mutex_unlock(&queue->mutex);
		exit(EXIT_FAILURE);
	}

	// Dequeue the message
	av_alist message = queue->work[queue->front];
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
	WorkerInfo* info = (WorkerInfo*) malloc(sizeof(WorkerInfo));
	info->worker_id = arrlen(download_workers) + 1;
	info->thread_id = 0;
	info->config = &_config;
	info->download_worker_shared = &_download_worker_shared;
	info->download_worker_instance = (DownloadWorkerInstance*) malloc(sizeof(DownloadWorkerInstance));

	pthread_create(&info->thread_id, NULL, start_download_worker, info);
	arrput(download_workers, info);
	update_worker_stats(WORKER_TYPE_DOWNLOAD, arrlen(download_workers));
}

void remove_download_worker()
{
	if (arrlen(download_workers) <= 0) {
		log_message(LOG_ERROR, "Couldn't remove download worker: download worker count <= 0");
		return;
	}

	WorkerInfo* info = arrpop(download_workers);
	pthread_cancel(info->thread_id);
	free(info);
	update_worker_stats(WORKER_TYPE_DOWNLOAD, arrlen(download_workers));
}


void add_render_worker()
{
	WorkerInfo* info = (WorkerInfo*) malloc(sizeof(WorkerInfo));
	info->worker_id = arrlen(render_workers) + 1;
	info->thread_id = 0;
	info->config = &_config;
	info->render_worker_shared = &_render_worker_shared;
	info->render_worker_instance = (RenderWorkerInstance*) malloc(sizeof(RenderWorkerInstance));

	pthread_create(&info->thread_id, NULL, start_render_worker, info);
	arrput(render_workers, info);
	update_worker_stats(WORKER_TYPE_RENDER, arrlen(render_workers));
}

void remove_render_worker()
{
	if (arrlen(render_workers) <= 0) {
		log_message(LOG_ERROR, "Couldn't remove render worker: render worker count <= 0");
		return;
	}

	WorkerInfo* info = arrpop(render_workers);
	pthread_cancel(info->thread_id);
	free(info);
	update_worker_stats(WORKER_TYPE_RENDER, arrlen(render_workers));
}

void add_save_worker()
{
	WorkerInfo* info = (WorkerInfo*) malloc(sizeof(WorkerInfo));
	info->worker_id = arrlen(save_workers) + 1;
	info->thread_id = 0;
	info->config = &_config;
	info->save_worker_shared = &_save_worker_shared;
	info->save_worker_instance = (SaveWorkerInstance*) malloc(sizeof(SaveWorkerInstance));

	pthread_create(&info->thread_id, NULL, start_save_worker, info);
	arrput(save_workers, info);
	update_worker_stats(WORKER_TYPE_SAVE, arrlen(save_workers));
}

void remove_save_worker()
{
	if (arrlen(save_workers) <= 0) {
		stop_console();
		log_message(LOG_ERROR, "Couldn't remove save worker: save worker count <= 0");
		exit(EXIT_FAILURE);
	}
	WorkerInfo* info = arrpop(save_workers);
	pthread_cancel(info->thread_id);
	free(info);
	update_worker_stats(WORKER_TYPE_SAVE, arrlen(save_workers));
}

void remove_all_workers(WorkerInfo** workers)
{
	for (int i = 0; i < arrlen(workers); i++) {
		WorkerInfo* worker = arrpop(workers);
		pthread_cancel(worker->thread_id);
		free(worker);
	}
}

WorkerInfo** get_workers(WorkerType type)
{
	if (type == WORKER_TYPE_DOWNLOAD) {
		return download_workers;
	}
	else if (type == WORKER_TYPE_RENDER) {
		return render_workers;
	}
	else if (type == WORKER_TYPE_SAVE) {
		return save_workers;
	}
	return NULL;
}

// Post queue
#define MAIN_THREAD_QUEUE_SIZE 64
MainThreadQueue work_queue = { };

// Public
// Enques work to work queue
void main_thread_post(av_alist work)
{
	push_work_queue(&work_queue, work);
}

// Called by main thread
void push_download_stack(CanvasInfo result)
{
	push_stack(&download_stack, &result);
}

// Called by download worker
void push_render_stack(DownloadResult result)
{
	push_stack(&render_stack, &result);
}

// Called by render worker
void push_save_stack(RenderResult result)
{
	push_stack(&save_stack, &result);
}

// Called by save worker
void push_completed(SaveResult result)
{
	// TODO: Reimplement this
	log_message(LOG_ERROR,  "TODO: FIXME: Implement push completed");

	completed_backups++;
	completed_backups_since++;
	av_alist backup_alist;
	av_start_void(backup_alist, &collect_backup_stats);
	main_thread_post(backup_alist);
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
DownloadResult pop_render_stack(int worker_id)
{
	DownloadResult result;
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

// STRICT: Called by main thread
void* read_commit_hashes(FILE* file)
{
	CanvasInfo new_canvas_info = { 0 };
	char line[MAX_HASHES_LINE_LEN];
	char* result = NULL;
	int line_index = 0;
	while ((result = fgets(line, MAX_HASHES_LINE_LEN, file)) != NULL) {
		line_index++;
		int result_len = strlen(result); // strip \n
		result[--result_len] = '\0';

		// Comment or ignore
		if (result_len == 0 || result[0] == '#' || result[0] == '\n') {
			log_message(LOG_INFO, LOG_HEADER"(Line %d) Ignoring comment '%s'", line_index, result);
			continue;
		}

		if (strncmp(result, "Commit: ", 8) == 0) {			
			int hash_len = result_len - 8;
			char* commit_hash = malloc(hash_len + 1);
			strcpy(commit_hash, result + 8);
			new_canvas_info.commit_hash = commit_hash;            
		}
		else if (strncmp(result, "Date: ", 6) == 0) {
			int date_len = strlen(result) - 6;
			char* date = malloc(date_len + 1);
			strcpy(date, result + 6);
			time_t date_int = strtoll(date, NULL, 10);
			new_canvas_info.date = date_int;

			// Proceed
			push_download_stack(new_canvas_info);
			memset(&new_canvas_info, 0, sizeof(CanvasInfo)); // Wipe for reuse

			if (download_stack.top >= STACK_SIZE_MAX - 1) {
				// We will come back later
				log_message(LOG_INFO, LOG_HEADER"Bufferred %d commit records into download stack. Pausing until needs replenish", download_stack.top + 1);
				break;
			}
		}
	}

	if (download_stack.top < 0) {
		stop_console();
		log_message(LOG_ERROR, "Could not find any unprocessed backups from commit_hashes.txt\n");
		exit(EXIT_SUCCESS);
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
		log_message(LOG_INFO, LOG_HEADER"Cloning progress: %d%% (%d/%d) - Indexed: %d", percentage, received_objects, total_objects, indexed_objects);
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
		log_message(LOG_ERROR, "Error creating revwalk: %s\n", e->message);
		return;
	}

	git_revwalk_sorting(walker, GIT_SORT_TIME);
	git_revwalk_push_head(walker);

	FILE* log_file = fopen(log_file_name, "a");
	if (!log_file) {
		log_message(LOG_ERROR, "Error opening log file for appending\n");
		git_revwalk_free(walker);
		return;
	}

	git_oid oid;
	int commit_count = 0;
	while (git_revwalk_next(&oid, walker) == 0) {
		git_commit* commit = NULL;
		if (git_commit_lookup(&commit, repo, &oid) != 0) {
			const git_error* e = git_error_last();
			log_message(LOG_ERROR, "Error looking up commit: %s\n", e->message);
			continue;
		}

		const char* commit_hash = git_oid_tostr_s(&oid);
		const char* author_name = git_commit_author(commit)->name;
		git_time_t commit_time = git_commit_time(commit);
		AUTOFREE char* commit_date = NULL;
		asprintf(&commit_date, "%ld", commit_time);

		fprintf(log_file, "Commit: %s\nAuthor: %s\nDate: %s\n", commit_hash, author_name, commit_date);
		commit_count++;

		if (commit_count % 100 == 0) {
			log_message(LOG_INFO, LOG_HEADER"Appended %d new commits...", commit_count);
		}

		git_commit_free(commit);
	}

	fclose(log_file);
	git_revwalk_free(walker);
	log_message(LOG_INFO, LOG_HEADER"Appended new commits to log file %s", log_file_name);
}

const char* get_repo_commit_hashes(const char* repo_url)
{
	log_message(LOG_INFO, LOG_HEADER"Initializing libgit2...");
	git_libgit2_init();

	const char* repo_path = "./repo";
	const char* log_file_name = "repo_commit_hashes.txt";

	if (is_repo_cloned(repo_path)) {
		log_message(LOG_INFO, LOG_HEADER"Repository already cloned.");

		if (is_commit_hashes_up_to_date(log_file_name)) {
			log_message(LOG_INFO, LOG_HEADER"Commit hashes log is up to date.");
			return log_file_name;
		}
		else {
			log_message(LOG_INFO, LOG_HEADER"Commit hashes log is outdated. Appending new commits...");
			git_repository* repo = NULL;
			if (git_repository_open(&repo, repo_path) != 0) {
				const git_error* e = git_error_last();
				log_message(LOG_ERROR, "Error opening repository: %s\n", e->message);
				git_libgit2_shutdown();
				return NULL;
			}
			append_new_commits_to_log(repo, log_file_name);
			git_repository_free(repo);
			return log_file_name;
		}
	}

	log_message(LOG_INFO, LOG_HEADER"Cloning repository from %s...", repo_url);

	git_repository* repo = NULL;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
	callbacks.transfer_progress = progress_callback;
	clone_opts.fetch_opts.callbacks = callbacks;

	log_message(LOG_INFO, LOG_HEADER"Starting repository clone...");
	if (git_clone(&repo, repo_url, repo_path, &clone_opts) != 0) {
		const git_error* e = git_error_last();
		log_message(LOG_ERROR, "Error cloning repository: %s\n", e->message);
		git_libgit2_shutdown();
		return NULL;
	}
	log_message(LOG_INFO, LOG_HEADER"Repository cloned successfully.");

	log_message(LOG_INFO, LOG_HEADER"Creating revwalk...");
	git_revwalk* walker = NULL;
	if (git_revwalk_new(&walker, repo) != 0) {
		const git_error* e = git_error_last();
		log_message(LOG_ERROR, "Error creating revwalk: %s\n", e->message);
		git_repository_free(repo);
		git_libgit2_shutdown();
		return NULL;
	}
	log_message(LOG_INFO, LOG_HEADER"Revwalk created successfully.");

	git_revwalk_sorting(walker, GIT_SORT_TIME);
	git_revwalk_push_head(walker);

	FILE* log_file = fopen(log_file_name, "w");
	if (!log_file) {
		log_message(LOG_ERROR, "Error opening log file for writing\n");
		git_revwalk_free(walker);
		git_repository_free(repo);
		git_libgit2_shutdown();
		return NULL;
	}
	log_message(LOG_INFO, LOG_HEADER"Log file %s created successfully.", log_file_name);

	git_oid oid;
	int commit_count = 0;
	while (git_revwalk_next(&oid, walker) == 0) {
		git_commit* commit = NULL;
		if (git_commit_lookup(&commit, repo, &oid) != 0) {
			const git_error* e = git_error_last();
			log_message(LOG_ERROR, "Error looking up commit: %s\n", e->message);
			continue;
		}

		const char* commit_hash = git_oid_tostr_s(&oid);
		const char* author_name = git_commit_author(commit)->name;
		git_time_t commit_time = git_commit_time(commit);
		AUTOFREE char* commit_date = NULL;
		asprintf(&commit_date, "%ld", commit_time);

		fprintf(log_file, "Commit: %s\nAuthor: %s\nDate: %s\n", commit_hash, author_name, commit_date);
		commit_count++;

		if (commit_count % 100 == 0) {
			log_message(LOG_INFO, LOG_HEADER"Processed %d commits...", commit_count);
		}

		git_commit_free(commit);
	}

	fclose(log_file);
	git_revwalk_free(walker);
	git_repository_free(repo);
	git_libgit2_shutdown();
	log_message(LOG_INFO, LOG_HEADER"Repository clone and log complete. Log saved to %s", log_file_name);

	char* result = strdup(log_file_name);
	return result;
}

int database_create_callback(void *data, int argc, char **argv, char **col_name)
{
	for (int i = 0; i < argc; i++) {
		log_message(LOG_INFO, LOG_HEADER"%s = %s\n", col_name[i], argv[i] ? argv[i] : "NULL");
	}
	return 0;
}

bool try_create_database(sqlite3* database)
{
	char* err_msg = NULL;
	const char* schema_file = "schema.sql";

	// Open database connection
	int result = sqlite3_open("instance_caches.db", &database);
	if (result != SQLITE_OK) {
		log_message(LOG_ERROR, LOG_HEADER"Cannot open database: %s\n", sqlite3_errmsg(database));
		return false;
	}

	// Read schema file
	FILE* file = fopen(schema_file, "r");
	if (!file) {
		log_message(LOG_ERROR, LOG_HEADER"Cannot open schema file: %s\n", schema_file);
		sqlite3_close(database);
		return false;
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	rewind(file);

	// Allocate memory to hold the schema content
	AUTOFREE char* sql = (char*)malloc(file_size + 1);
	if (sql == NULL) {
		log_message(LOG_ERROR, LOG_HEADER"Memory allocation failed\n");
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
		log_message(LOG_ERROR, LOG_HEADER"Failed to create database: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(database);
		return false;
	}

	return true;
}

// Start all workers, initiate rendering backups
void start_generation(Config config)
{
	// Create database
	if (!try_create_database(_database)) {
		stop_console();
		log_message(LOG_ERROR, LOG_HEADER"Error creating database\n");
		exit(EXIT_FAILURE);
	}

	// Create curl
	_config = config;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	CURL* curl = curl_easy_init();
	if (curl == NULL) {
		stop_console();
		log_message(LOG_ERROR, LOG_HEADER"Error initialising curl\n");
		exit(EXIT_FAILURE);
	}

	// Create shared worker datas
	_download_worker_shared = (DownloadWorkerShared) { };
	_render_worker_shared = (RenderWorkerShared) { };
	_save_worker_shared = (SaveWorkerShared) { };

	// Either source commit info from commit hashes or repo URL
	log_message(LOG_INFO, LOG_HEADER"Starting generation process...");

	const char* log_file_name = config.commit_hashes_file_name;
	if (config.repo_url) {
		log_file_name = get_repo_commit_hashes(config.repo_url);
	}
	if (!log_file_name) {
		stop_console();
		log_message(LOG_ERROR, LOG_HEADER"Couldn't locate commit hashes file\n");
		exit(EXIT_FAILURE);
	}

	log_message(LOG_INFO, LOG_HEADER"Opening log file %s for reading...", log_file_name);
	FILE* file = fopen(log_file_name, "r");
	if (file == NULL) {
		stop_console();
		log_message(LOG_ERROR, LOG_HEADER"Couldn't locate commit hashes file (%s)\n", log_file_name);
		exit(EXIT_FAILURE);
	}
	commit_hashes_stream = file;

	long file_lines = flines(file);
	log_message(LOG_INFO, LOG_HEADER"Detected %d lines in %s", file_lines, log_file_name);
	read_commit_hashes(file);

	// Create required directories
	if (mkdir("backups", 0777) == -1) {
		if (errno != EEXIST) {
			stop_console();
			log_message(LOG_ERROR, LOG_HEADER, "Error creating backups directory\n");
			exit(EXIT_FAILURE);
		}
	}
	if (mkdir("dates", 0777) == -1) {
		if (errno != EEXIST) {
			stop_console();
			log_message(LOG_ERROR, LOG_HEADER, "Error creating dates directory\n");
			exit(EXIT_FAILURE);
		}
	}

	log_message(LOG_INFO, LOG_HEADER"Starting backup generation...");
	for (int i = 0; i < DEFAULT_DOWNLOAD_WORKER_COUNT; i++) {
		log_message(LOG_INFO, LOG_HEADER"Adding download worker %d...", i + 1);
		add_download_worker();
	}
	for (int i = 0; i < DEFAULT_RENDER_WORKER_COUNT; i++) {
		log_message(LOG_INFO, LOG_HEADER"Adding render worker %d...", i + 1);
		add_render_worker();
	}
	for (int i = 0; i < DEFAULT_SAVE_WORKER_COUNT; i++) {
		log_message(LOG_INFO, LOG_HEADER"Adding save worker %d...", i + 1);
		add_save_worker();
	}
	log_message(LOG_INFO, LOG_HEADER"Backup generation started.");
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
	remove_all_workers(download_workers);
	remove_all_workers(render_workers);
	remove_all_workers(save_workers);

	// Cleanup globals
	curl_global_cleanup();
	pthread_exit(NULL);
	exit(0);
}

void safe_segfault_exit(int sig_num)
{
	size_t size = 0;
	void* stack_pointers[10];

	// get void*'s for all entries on the stack
	size = backtrace(stack_pointers, 10);

	// print out all the frames to stderr
	log_message(LOG_ERROR, "Error: signal %d:\n", sig_num);
	backtrace_symbols_fd(stack_pointers, size, STDERR_FILENO);

	stop_console();
	log_message(LOG_ERROR, "Fatal - Segmentation fault\n");

	exit(EXIT_FAILURE);
}

void start_main_thread(bool start, Config config)
{
	signal(SIGSEGV, safe_segfault_exit);
	completed_backups_date = time(0);

	init_work_queue(&work_queue, MAIN_THREAD_QUEUE_SIZE);
	init_stack(&download_stack, sizeof(CanvasInfo), STACK_SIZE_MAX);
	init_stack(&render_stack, sizeof(DownloadResult), STACK_SIZE_MAX);
	init_stack(&save_stack, sizeof(RenderResult), STACK_SIZE_MAX);

	if (start) {
		start_generation(config);
	}
	while (true) {
		// Will wait for work to arrive via work queue, at which point
		// main thread will pop & process work
		while (!work_queue_replenished) {
			usleep(10000); // Wait for 10ms
		}
		work_queue_replenished = false;

		av_alist work = pop_work_queue(&work_queue);
		av_call(work);
	}
}

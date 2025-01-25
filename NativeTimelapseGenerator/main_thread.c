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
#include "database.h"
#define STB_DS_IMPLEMENTATION
#include "lib/stb/stb_ds.h"

#include "workers/worker_structs.h"
#include "workers/download_worker.h"
#include "workers/render_worker.h"
#include "workers/save_worker.h"

// Reads canvas list, manages jobs for workers
#define LOG_HEADER "[main thread] "

// SHARED BETWEEN-WORKER MEMORY
Stack download_stack;
Stack render_stack;
Stack save_stack;

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

// CONFIG
Config _config = { 0 };

// WORKER DATAS
DownloadWorkerShared _download_worker_shared = {
	.user_map = NULL
};
RenderWorkerShared _render_worker_shared;
SaveWorkerShared _save_worker_shared;


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
WorkQueue main_thread_work_queue;

// Public
// Enques work to work queue
void main_thread_post(av_alist work)
{
	push_work_queue(&main_thread_work_queue, work);
}

// Called by main thread
void push_download_stack(DownloadJob job)
{
	push_stack(&download_stack, &job);
}

// Called by download worker
void push_render_stack(RenderJob job)
{
	push_stack(&render_stack, &job);
}

// Called by render worker
void push_save_stack(SaveJob job)
{
	push_stack(&save_stack, &job);
}

void collect_backup_stats()
{
	// TODO: Implement this
	/*time_t now = time(0);
	float backups_per_second = ((float)(now - completed_backups_date)) / (float) completed_backups_since;
	update_backups_stats(completed_backups, backups_per_second, *completed_canvas_info);
	completed_backups_since = 0;
	completed_backups_date = now;*/
}

// Called by save worker
void push_completed(SaveResult result)
{
	// TODO: Implement this
	completed_backups++;
	completed_backups_since++;
	av_alist backup_alist;
	av_start_void(backup_alist, &collect_backup_stats);
	main_thread_post(backup_alist);
}

// Forward declarations
void* read_commit_hashes(int instance_id, FILE* file);
FILE* commit_hashes_stream = NULL;

// Called by download worker
DownloadJob pop_download_stack(int worker_id)
{
	DownloadJob result = { 0 };
	while (!pop_stack(&download_stack, &result)) {
		usleep(10000); // Wait for 10ms
	}
	return result;
}

// Called by render worker
RenderJob pop_render_stack(int worker_id)
{
	RenderJob result = { 0 };
	while (!pop_stack(&render_stack, &result)) {
		usleep(10000); // Wait for 10ms
	}
	return result;
}

// Called by save worker
SaveJob pop_save_stack(int worker_id)
{
	SaveJob result = { 0 };
	while (!pop_stack(&save_stack, &result)) {
		usleep(10000); // Wait for 10ms
	}
	return result;
}

// STRICT: Called by main thread
void designate_jobs(int commit_id, CommitInfo info)
{
	// Check canvas download and rendering
	if (!check_save_exists(commit_id, SAVE_CANVAS_DOWNLOAD)) {
		DownloadJob download_canvas_job = {
			.commit_id = commit_id,
			.commit_hash = info.commit_hash,
			.date = info.date,
			.type = DOWNLOAD_CANVAS
		};
		push_download_stack(download_canvas_job);
	}
	/*else if (!check_save_exists(commit_id, SAVE_CANVAS_RENDER)) {
		// If canvas is downloaded but not rendered, render it
		RenderJob render_canvas_job = {
			.commit_id = commit_id,
			.commit_hash = info.commit_hash,
			.date = info.date,
			.type = RENDER_CANVAS,
			.canvas = {
				
			}
		};
		push_render_stack(render_canvas_job);
	}*/

	// Check placers download and rendering
	if (!check_save_exists(commit_id, SAVE_PLACERS_DOWNLOAD)) {
		DownloadJob download_placers_job = {
			.commit_id = commit_id,
			.commit_hash = info.commit_hash,
			.date = info.date,
			.type = DOWNLOAD_PLACERS
		};
		push_download_stack(download_placers_job);
	}
	else {
		// If placers are downloaded, check related renders
		/*if (!check_save_exists(commit_id, SAVE_TOP_PLACERS_RENDER)) {
			RenderJob top_placers_job = {
				.commit_id = commit_id,
				.commit_hash = info.commit_hash,
				.date = info.date,
				.type = RENDER_TOP_PLACERS
			};
			push_render_stack(top_placers_job);
		}

		if (!check_save_exists(commit_id, SAVE_CANVAS_CONTROL_RENDER)) {
			RenderJob canvas_control_job = {
				.commit_id = commit_id,
				.commit_hash = info.commit_hash,
				.date = info.date,
				.type = RENDER_CANVAS_CONTROL
			};
			push_render_stack(canvas_control_job);
		}*/
	}

	// Check date rendering
	if (!check_save_exists(commit_id, SAVE_DATE_RENDER)) {
		RenderJob render_date_job = {
			.commit_id = commit_id,
			.commit_hash = info.commit_hash,
			.date = info.date,
			.type = RENDER_DATE
		};
		push_render_stack(render_date_job);
	}
}

// STRICT: Called by main thread
void* read_commit_hashes(int instance_id, FILE* file)
{
	CommitInfo new_canvas_info = { 0 };
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

			int commit_id = add_commit_to_db(instance_id, new_canvas_info);
			if (commit_id == -1) {
				log_message(LOG_ERROR, LOG_HEADER"Failed to add commit to database: %s", 
					new_canvas_info.commit_hash, new_canvas_info.commit_hash);
			}
			else {
				// Push collected infos to the stacks to be processed
				designate_jobs(commit_id, new_canvas_info);
			}

			// Wipe for reuse
			memset(&new_canvas_info, 0, sizeof(CommitInfo));

			// We can buffer more (stack will dynamically resize), but we will pause here to allow other 
			// jobs a chance to run on main thread
			if (download_stack.top > DEFAULT_STACK_SIZE) {
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

void make_save_dir(const char* name)
{
	if (mkdir(name, 0777) == -1) {
		if (errno != EEXIST) {
			stop_console();
			log_message(LOG_ERROR, LOG_HEADER, "Error creating %s directory\n", name);
			exit(EXIT_FAILURE);
		}
	}
}

// Start all workers, initiate rendering backups
void start_generation(Config config)
{
	// Start database work thread
	start_database();

	// Create database
	if (!try_create_database()) {
		stop_console();
		log_message(LOG_ERROR, LOG_HEADER"Error creating database\n");
		exit(EXIT_FAILURE);
	}

	// Add new instance to DB
	int instance_id = find_existing_instance(&config);;
	if (instance_id == -1 && !add_instance_to_db(&config)) {
		stop_console();
		log_message(LOG_ERROR, LOG_HEADER"Error adding instance to database\n");
		exit(EXIT_FAILURE);
	}
	instance_id = get_last_instance_id();

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
	if (config.repo_url && strlen(config.repo_url) > 0) {
		log_file_name = get_repo_commit_hashes(config.repo_url);
	}
	if (!log_file_name || strlen(log_file_name) <= 0) {
		log_message(LOG_ERROR, LOG_HEADER"Couldn't locate commit hashes file\n");
		stop_console();
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
	read_commit_hashes(instance_id, file);

	// Create required directories
	make_save_dir("canvas_downloads");
	make_save_dir("placer_downloads");
	make_save_dir("canvas_renders");
	make_save_dir("date_renders");
	make_save_dir("top_placer_renders");
	make_save_dir("canvas_control_renders");

	// Start workers
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
	free_work_queue(&main_thread_work_queue);	

	// Cleanup stacks
	free_stack(&download_stack);
	free_stack(&render_stack);
	free_stack(&save_stack);

	// Terminate and cleanup all workers
	remove_all_workers(download_workers);
	remove_all_workers(render_workers);
	remove_all_workers(save_workers);

	log_message(LOG_INFO, "Backup generation stopped.");
}

void stop_global()
{
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

	init_work_queue(&main_thread_work_queue, DEFAULT_WORK_QUEUE_SIZE);
	init_stack(&download_stack, sizeof(DownloadJob), DEFAULT_STACK_SIZE);
	init_stack(&render_stack, sizeof(RenderJob), DEFAULT_STACK_SIZE);
	init_stack(&save_stack, sizeof(SaveJob), DEFAULT_STACK_SIZE);

	if (start) {
		start_generation(config);
	}
	while (true) {
		// Will wait for work to arrive via work queue, at which point
		// main thread will pop & process work
		while (!main_thread_work_queue.replenished) {
			usleep(10000); // Wait for 10ms
		}

		av_alist work = pop_work_queue(&main_thread_work_queue);
		av_call(work);
	}
}

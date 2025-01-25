#pragma once
#include <avcall.h>
#include <sqlite3.h>
#include "main_thread.h"
#include "workers/worker_structs.h"

// BETTER: Call on database thread for non-blocking
bool add_save_to_db(int commit_id, SaveJobType type, const char* save_path);
// BETTER: Call on database thread for non-blocking
bool check_save_exists(int commit_id, SaveJobType type);
// BETTER: Call on database thread for non-blocking
bool add_canvas_metadata_to_db(CanvasMetadata metadata, int commit_id);
// BETTER: Call on database thread for non-blocking
int add_commit_to_db(int instance_id, CommitInfo info);
// BETTER: Call on database thread for non-blocking
int find_existing_instance(const Config* config);
// BETTER: Call on database thread for non-blocking
bool add_instance_to_db(const Config* config);
// BETTER: Call on database thread for non-blocking
int get_last_instance_id();
// BETTER: Call on database thread for non-blocking
bool try_create_database();

void database_thread_post(av_alist work);
void start_database();
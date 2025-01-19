#pragma once
#include <avcall.h>
#include <sqlite3.h>
#include "main_thread.h"

// BETTER: Call on database thread for non-blocking
bool add_commit_to_db(int instance_id, const char* hash, time_t date);
// BETTER: Call on database thread for non-blocking
bool populate_commits_db(int instance_id, FILE* file);
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
#include <avcall.h>
#include <pthread.h>
#include <sqlite3.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <string.h>

#include "main_thread.h"
#include "console.h"
#include "memory_utils.h"
#include "workers/worker_structs.h"
#include "database.h"

#include "lib/stb/stb_ds.h"

#define LOG_HEADER "[database] "

// DATABASE
static sqlite3* database = NULL;
static pthread_t database_thread_id;
// Needed until Work functions are made awaitable
static pthread_mutex_t database_mutex;

void compute_palette_hash(const Colour* palette, int palette_size, char* out_hash)
{
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	unsigned char hash[EVP_MAX_MD_SIZE];
	
	EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);

	for (int i = 0; i < palette_size; i++) {
		EVP_DigestUpdate(ctx, &palette[i], sizeof(Colour));
	}

	unsigned int hash_len = 0;
	EVP_DigestFinal_ex(ctx, hash, &hash_len);
	EVP_MD_CTX_free(ctx);
	
	for (unsigned int i = 0; i < hash_len; i++) {
		sprintf(out_hash + (i * 2), "%02x", hash[i]);
	}
	out_hash[hash_len * 2] = '\0';
}

int create_new_palette(const Colour* palette, int palette_size)
{
	sqlite3_stmt* stmt;
	int palette_id = -1;
	
	// Insert new palette
	const char* sql_insert_palette = "INSERT INTO Palettes (size, hash) VALUES (?, ?)";
	if (sqlite3_prepare_v2(database, sql_insert_palette, -1, &stmt, NULL) != SQLITE_OK) {
		return -1;
	}

	sqlite3_bind_int(stmt, 1, palette_size);

	char palette_hash[SHA256_DIGEST_LENGTH * 2 + 1];
	compute_palette_hash(palette, palette_size, palette_hash);
	sqlite3_bind_text(stmt, 2, palette_hash, -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_finalize(stmt);
	
	palette_id = sqlite3_last_insert_rowid(database);
	
	// Insert colors for the palette
	const char* sql_insert_colors = 
		"INSERT INTO Colours (palette_id, red, green, blue, alpha, position) "
		"VALUES (?, ?, ?, ?, ?, ?)";
	
	if (sqlite3_prepare_v2(database, sql_insert_colors, -1, &stmt, NULL) != SQLITE_OK) {
		return -1;
	}
	
	for (int i = 0; i < palette_size; i++) {
		sqlite3_bind_int(stmt, 1, palette_id);
		sqlite3_bind_int(stmt, 2, palette[i].r);
		sqlite3_bind_int(stmt, 3, palette[i].g);
		sqlite3_bind_int(stmt, 4, palette[i].b);
		sqlite3_bind_int(stmt, 5, palette[i].a);
		sqlite3_bind_int(stmt, 6, i);
		
		if (sqlite3_step(stmt) != SQLITE_DONE) {
			sqlite3_finalize(stmt);
			return -1;
		}
		sqlite3_reset(stmt);
	}
	
	sqlite3_finalize(stmt);
	return palette_id;
}

int find_existing_palette(const Colour* palette, int palette_size)
{
	char palette_hash[SHA256_DIGEST_LENGTH * 2 + 1];
	compute_palette_hash(palette, palette_size, palette_hash);

	sqlite3_stmt* stmt;
	const char* sql = "SELECT id FROM Palettes WHERE hash = ?";
	int palette_id = -1;

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return -1;
	}

	sqlite3_bind_text(stmt, 1, palette_hash, -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		palette_id = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	return palette_id;
}

int find_existing_canvas_metadata(CanvasMetadata metadata, int palette_id)
{
	sqlite3_stmt* stmt;
	int metadata_id = -1;
	
	const char* sql = 
		"SELECT id FROM CanvasMetadatas "
		"WHERE width = ? AND height = ? AND palette_id = ?";
	
	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return -1;
	}
	
	sqlite3_bind_int(stmt, 1, metadata.width);
	sqlite3_bind_int(stmt, 2, metadata.height);
	sqlite3_bind_int(stmt, 3, palette_id);
	
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		metadata_id = sqlite3_column_int(stmt, 0);
	}
	
	sqlite3_finalize(stmt);
	return metadata_id;
}

// Main function to add canvas metadata to database
bool add_canvas_metadata_to_db(CanvasMetadata metadata, int commit_id)
{
	// Begin transaction
	if (sqlite3_exec(database, "BEGIN TRANSACTION", NULL, NULL, NULL) != SQLITE_OK) {
		log_message(LOG_ERROR, LOG_HEADER"Failed to begin transaction\n");
		return false;
	}

	// First, handle the palette
	int palette_id = find_existing_palette(metadata.palette, metadata.palette_size);
	if (palette_id == -1) {
		palette_id = create_new_palette(metadata.palette, metadata.palette_size);
		if (palette_id == -1) {
			log_message(LOG_ERROR, LOG_HEADER"Failed to create new palette\n");
			sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
			return false;
		}
	}

	// Check if canvas metadata already exists
	int metadata_id = find_existing_canvas_metadata(metadata, palette_id);

	if (metadata_id == -1) {
		// Create new canvas metadata
		sqlite3_stmt* stmt;
		const char* sql = 
			"INSERT INTO CanvasMetadatas (palette_id, first_seen_commit_id, width, height) "
			"VALUES (?, ?, ?, ?)";
		
		if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
			sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
			return false;
		}
		
		sqlite3_bind_int(stmt, 1, palette_id);
		sqlite3_bind_int(stmt, 2, commit_id);
		sqlite3_bind_int(stmt, 3, metadata.width);
		sqlite3_bind_int(stmt, 4, metadata.height);
		
		if (sqlite3_step(stmt) != SQLITE_DONE) {
			sqlite3_finalize(stmt);
			sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
			return false;
		}
		
		sqlite3_finalize(stmt);
		metadata_id = sqlite3_last_insert_rowid(database);
	}
	
	// Link the commit to the canvas metadata
	sqlite3_stmt* stmt;
	const char* sql = 
		"INSERT INTO CommitCanvasMetadatas (commit_id, canvas_metadata_id) "
		"VALUES (?, ?)";
	
	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
		return false;
	}
	
	sqlite3_bind_int(stmt, 1, commit_id);
	sqlite3_bind_int(stmt, 2, metadata_id);
	
	int success = false;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		success = true;
	}
	
	sqlite3_finalize(stmt);
	
	// Commit or rollback transaction
	if (success) {
		if (sqlite3_exec(database, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
			log_message(LOG_ERROR, LOG_HEADER"Failed to commit transaction\n");
			success = false;
		}
	}
	else {
		sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
	}
	
	return success;
}

bool add_commit_to_db(int instance_id, const char* hash, time_t date)
{
	pthread_mutex_lock(&database_mutex);

	const char* sql = "INSERT INTO Commits (instance_id, hash, date) VALUES (?, ?, ?);";
	sqlite3_stmt* stmt;
	int rc;

	rc = sqlite3_prepare_v2(database, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		log_message(LOG_ERROR, LOG_HEADER"Failed to prepare statement: %s\n", sqlite3_errmsg(database));
		pthread_mutex_unlock(&database_mutex);
		return false;
	}

	sqlite3_bind_int(stmt, 1, instance_id);
	sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 3, date);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		log_message(LOG_ERROR, LOG_HEADER"Failed to insert commit: %s\n", sqlite3_errmsg(database));
		pthread_mutex_unlock(&database_mutex);
		return false;
	}

	pthread_mutex_unlock(&database_mutex);
	return true;
}

bool populate_commits_db(int instance_id, FILE* file)
{
	rewind(file); // Reset file pointer to beginning
	
	char line[MAX_HASHES_LINE_LEN];
	char* result = NULL;
	char* current_hash = NULL;
	time_t current_date = 0;
	bool success = true;

	while ((result = fgets(line, MAX_HASHES_LINE_LEN, file)) != NULL) {
		int result_len = strlen(result);
		result[--result_len] = '\0';  // Remove newline

		// Skip comments and empty lines
		if (result_len <= 0 || result[0] == '#' || result[0] == '\n') {
			continue;
		}

		if (strncmp(result, "Commit: ", 8) == 0) {
			current_hash = strdup(result + 8);
		}
		else if (strncmp(result, "Date: ", 6) == 0) {
			current_date = strtoll(result + 6, NULL, 10);
			
			if (current_hash && current_date) {
				if (!add_commit_to_db(instance_id, current_hash, current_date)) {
					log_message(LOG_ERROR, LOG_HEADER"Failed to add commit to database: %s at %ld\n", 
						current_hash, current_date);
					success = false;
				}
				free(current_hash);
				current_hash = NULL;
				current_date = 0;
			}
		}
	}

	if (current_hash) {
		free(current_hash);
	}

	return success;
}

int find_existing_instance(const Config* config)
{
	pthread_mutex_lock(&database_mutex);
	int instance_id = -1;
	sqlite3_stmt* stmt;

	const char* sql = 
		"SELECT id FROM Instances "
		"WHERE repo_url = ? AND game_server_url = ?";
	
	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		pthread_mutex_unlock(&database_mutex);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, config->repo_url, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, config->game_server_base_url, -1, SQLITE_STATIC);
	
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		instance_id = sqlite3_column_int(stmt, 0);
	}
	
	sqlite3_finalize(stmt);
	pthread_mutex_unlock(&database_mutex);
	return instance_id;
}

bool add_instance_to_db(const Config* config)
{
	pthread_mutex_lock(&database_mutex);
	if (find_existing_instance(config)) {
		log_message(LOG_ERROR, LOG_HEADER"Failed to insert instance: Matching instance already present in DB");
		return false;
	}

	const char* sql = "INSERT INTO Instances (repo_url, game_server_url) VALUES (?, ?);";
	sqlite3_stmt* stmt;
	int rc;

	rc = sqlite3_prepare_v2(database, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		log_message(LOG_ERROR, LOG_HEADER"Failed to prepare statement: %s\n", sqlite3_errmsg(database));
		pthread_mutex_unlock(&database_mutex);
		return false;
	}

	sqlite3_bind_text(stmt, 1, config->repo_url, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, config->game_server_base_url, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		log_message(LOG_ERROR, LOG_HEADER"Failed to insert instance: %s\n", sqlite3_errmsg(database));
		pthread_mutex_unlock(&database_mutex);
		return false;
	}

	pthread_mutex_unlock(&database_mutex);
	return true;
}

int get_last_instance_id()
{
	pthread_mutex_lock(&database_mutex);
	int id = sqlite3_last_insert_rowid(database);
	pthread_mutex_unlock(&database_mutex);
	return id;
}

int database_create_callback(void* data, int argc, char** argv, char** col_name)
{
	for (int i = 0; i < argc; i++) {
		log_message(LOG_INFO, LOG_HEADER"%s = %s\n", col_name[i], argv[i] ? argv[i] : "NULL");
	}
	return 0;
}

bool try_create_database()
{
	pthread_mutex_lock(&database_mutex);
	char* err_msg = NULL;
	const char* schema_file = "schema.sql";

	// Open database connection
	int result = sqlite3_open("instance_caches.db", &database);
	if (result != SQLITE_OK) {
		log_message(LOG_ERROR, LOG_HEADER"Cannot open database: %s\n", sqlite3_errmsg(database));
		pthread_mutex_unlock(&database_mutex);
		return false;
	}

	// Read schema file
	FILE* file = fopen(schema_file, "r");
	if (!file) {
		log_message(LOG_ERROR, LOG_HEADER"Cannot open schema file: %s\n", schema_file);
		sqlite3_close(database);
		pthread_mutex_unlock(&database_mutex);
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
		pthread_mutex_unlock(&database_mutex);
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
		pthread_mutex_unlock(&database_mutex);
		return false;
	}

	pthread_mutex_unlock(&database_mutex);
	return true;
}

WorkQueue database_thread_work_queue;

void database_thread_post(av_alist work)
{
	push_work_queue(&database_thread_work_queue, work);
}

void* start_db_work_loop(void* data)
{
	while (true) {
		while (!database_thread_work_queue.replenished) {
			usleep(10000); // Wait for 10ms
		}

		av_alist work = pop_work_queue(&database_thread_work_queue);
		av_call(work);
	}
}

void start_database()
{
	pthread_mutex_init(&database_mutex, NULL);
	init_work_queue(&database_thread_work_queue, DEFAULT_WORK_QUEUE_SIZE);
	pthread_create(&database_thread_id, NULL, start_db_work_loop, NULL);
}

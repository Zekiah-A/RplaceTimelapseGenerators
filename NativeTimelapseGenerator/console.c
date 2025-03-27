#include "lib/libnanobuf/buf_writer.h"
#include "lib/libnanobuf/nanobuf_defs.h"
#include "workers/worker_structs.h"
#include <linux/limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <avcall.h>

#include "lib/libdill/libdill.h"
#include "lib/libnanobuf/nanobuf.h"
#include "lib/stb/stb_ds.h"

#include "console.h"
#include "main_thread.h"
#include "memory_utils.h"

void (*add_funcs[3])() = { add_download_worker, add_render_worker, add_save_worker };
void (*remove_funcs[3])() = { remove_download_worker, remove_render_worker, remove_save_worker };

int* event_sockets = NULL;
static pthread_mutex_t event_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOG_HEADER "[console] "

typedef enum event_packet:uint8_t {
	EVENT_PACKET_LOG_MESSAGE = 0,
	EVENT_PACKET_WORKERS_INFO = 1,
	EVENT_PACKET_SAVE_STATUS = 2,
	EVENT_PACKET_START_STATUS = 3
} EventPacket;

typedef enum control_packet:uint8_t {
	CONTROL_PACKET_START = 16,
	CONTROL_PACKET_STOP = 17,
	CONTROL_PACKET_ADD_WORKER = 18,
	CONTROL_PACKET_REMOVE_WORKER = 19
} ControlPacket;

typedef struct log_message {
	LogType type;
	time_t date;
	char* message;
} LogMessage;
LogMessage* log_messages = NULL;

pthread_t repl_thread_id;
pthread_t socket_thread_id;
volatile sig_atomic_t repl_keep_running = true;
volatile bool socket_keep_running = true;

// Called by UI therad, must pass back to main thread
NOSANITIZE void ui_start_generation(Config config)
{
	av_alist start_alist;
	av_start_void(start_alist, &start_generation);
	av_struct(start_alist, Config, config); 
	main_thread_post(start_alist);
}

// Called by UI thread, must pass back to main thread
NOSANITIZE void ui_stop_generation()
{
	av_alist stop_alist;
	av_start_void(stop_alist, &stop_generation); 
	main_thread_post(stop_alist);
}

// Called by UI thread, must pass back to main thread
NOSANITIZE void ui_add_worker(int worker_type, int add)
{
	for (int i = 0; i < add; i++) {
		av_alist add_worker_alist;
		av_start_void(add_worker_alist, &add_funcs[worker_type]); 
		main_thread_post(add_worker_alist);
	}
}

// Called by UI thread, must pass back to main thread
NOSANITIZE void ui_remove_worker(int worker_type, int remove)
{
	for (int i = 0; i < remove; i++) {
		av_alist rm_worker_alist;
		av_start_void(rm_worker_alist, &remove_funcs[worker_type]); 
		main_thread_post(rm_worker_alist);
	}
}

void ui_exit()
{
	log_message(LOG_INFO, "NativeTimelapseGenerator exiting...");

	ui_stop_generation();
	// HACK: There is no functionality yet to await a main thread post so we do this 
	usleep(500000);

	stop_console();
	stop_global();
}

int str_ends_with(const char* str, const char* suffix)
{
	if (!str || !suffix) {
		return 0;
	}
	size_t string_length = strlen(str);
	size_t suffix_length = strlen(suffix);
	if (suffix_length > string_length) {
		return 0;
	}

	return strncmp(str + string_length - suffix_length, suffix, suffix_length) == 0;
}

coroutine void ws_send_all_packet(BufWriter* packet)
{
	if (!socket_keep_running) {
		return;
	}

	uint8_t* data = packet->start;
	size_t packet_size = bw_size(packet);
	time_t deadline = now() + 5000;

	int* current_sockets = NULL;
	pthread_mutex_lock(&event_sockets_mutex);
	arrsetcap(current_sockets, arrlen(event_sockets));
	for (int i = 0; i < arrlen(event_sockets); ++i) {
		arrput(current_sockets, event_sockets[i]);
	}
	pthread_mutex_unlock(&event_sockets_mutex);

	for (int i = 0; i < arrlen(current_sockets); ++i) {
		int sock = current_sockets[i];
		if (sock > 0) {
			int rc = msend(sock, data, packet_size, deadline);
			if (rc < 0) {
				continue;
			}
		}
	}
	arrfree(current_sockets);
}

// Hoisted definition
coroutine void ws_disconnect(int socket);

coroutine void ws_send_packet(int socket, BufWriter* packet)
{
	if (!socket_keep_running) {
		return;
	}
	time_t deadline = now() + 5000;
	int rc = msend(socket, packet->start, bw_size(packet), deadline);
	if (rc < 0) {
		ws_disconnect(socket);
	}
}

void write_worker_info(BufWriter* packet, WorkerInfo* worker)
{
	bw_u8(packet, worker->worker_id);
	bw_u8(packet, worker->status);
}

void write_worker_infos(BufWriter* packet, WorkerType type)
{
	WorkerInfo** workers = get_workers(type);
	bw_u8(packet, type);
	bw_u8(packet, arrlen(workers));

	for (int i = 0; i < arrlen(workers); i++) {
		WorkerInfo* worker = workers[i];
		write_worker_info(packet, worker);
	}
}

coroutine void ws_listen(int socket)
{
	char buf[1024];
	bool connection_alive = true;
	int consecutive_errors = 0;
	const int max_consecutive_errors = 3;

	while (socket_keep_running && connection_alive) {
		memset(buf, 0, sizeof(buf));

		// Receive a WebSocket packet (will block indefinately)
		size_t size = mrecv(socket, buf, sizeof(buf), -1);

		// Handle disconnection or errors
		if (size < 0) {
			consecutive_errors++;

			// Handle timeout specially
			if (errno == ETIMEDOUT) {
				if (consecutive_errors >= max_consecutive_errors) {
					log_message(LOG_INFO, "Connection appears dead after multiple timeouts");
					connection_alive = false;
				}
				continue;
			}			
			if (errno == ECONNRESET || errno == EPIPE || errno == ECONNABORTED || errno == ENOTCONN) {
				log_message(LOG_INFO, "WebSocket disconnected (errno: %d - %s)", errno, strerror(errno));
				connection_alive = false;
				break;
			}
			log_message(LOG_ERROR, "Socket error: %d - %s", errno, strerror(errno));
			if (consecutive_errors >= max_consecutive_errors) {
				log_message(LOG_ERROR, "Too many consecutive errors, closing connection");
				connection_alive = false;
			}
			continue;
		}
		// Ignore empty packets
		if (size == 0) {
			continue;
		}

		consecutive_errors = 0;
		BufReader packet = br_from_ptr(&buf, size, NULL);
		ControlPacket type = br_u8(&packet);
		switch (type) {
			case CONTROL_PACKET_START: {
				Config config = { 0 };
				// Nullable
				char* repo_url_str = nb_to_cstr(br_str(&packet));
				config.repo_url =  (repo_url_str && strlen(repo_url_str) > 0) >  0 ? repo_url_str : NULL;
				
				config.download_base_url = nb_to_cstr(br_str(&packet));
				config.game_server_base_url = nb_to_cstr(br_str(&packet));
				
				// Nullable
				char* commit_hashes_str = nb_to_cstr(br_str(&packet));
				config.commit_hashes_file_name = (commit_hashes_str && strlen(commit_hashes_str) > 0) ? commit_hashes_str : NULL;
				
				config.max_top_placers = br_u32(&packet);
				ui_start_generation(config);
				break;
			}
			case CONTROL_PACKET_STOP: {
				ui_stop_generation();
				break;
			}
			case CONTROL_PACKET_ADD_WORKER: {
				WorkerType type = br_u8(&packet);
				int count = br_u8(&packet);
				ui_add_worker(type, count);
				break;
			}
			case CONTROL_PACKET_REMOVE_WORKER: {
				WorkerType type = br_u8(&packet);
				int count = br_u8(&packet);
				ui_remove_worker(type, count);
				break;
			}
			default: {
				log_message(LOG_WARNING, "Unknown control packet type: %d", type);
				connection_alive = false;
				break;
			}
		}
	}

	// Clean up when the connection is closed
	ws_disconnect(socket);
}

coroutine void ws_connect(int socket)
{
	socket = ws_attach_server(socket, WS_BINARY | WS_NOHTTP, NULL, 0, NULL, 0, -1);
	if (socket < 0) {
		log_message(LOG_ERROR, "Error attaching WS server: %d - %s", errno, strerror(errno));
		return;
	}
	pthread_mutex_lock(&event_sockets_mutex);
	arrput(event_sockets, socket);
	pthread_mutex_unlock(&event_sockets_mutex);

	// Start listening
	go(ws_listen(socket));

	// Send logs history
	for (int i = 0; i < arrlen(log_messages); i++) {
		LogMessage log_message = log_messages[i];
		bw_stackfree(packet) = bw_create_default();
		bw_u8(&packet, EVENT_PACKET_LOG_MESSAGE);
		bw_u8(&packet, log_message.type);
		bw_u64(&packet, log_message.date);
		bw_str(&packet, log_message.message);
		ws_send_packet(socket, &packet);
	}
	log_message(LOG_INFO, "Event socket client connected!");

	// Send all workers info
	bw_stackfree(packet) = bw_create_default();
	bw_u8(&packet, EVENT_PACKET_WORKERS_INFO);
	bw_u8(&packet, 3); // workers count
	write_worker_infos(&packet, WORKER_TYPE_DOWNLOAD);
	write_worker_infos(&packet, WORKER_TYPE_RENDER);
	write_worker_infos(&packet, WORKER_TYPE_SAVE);
	ws_send_packet(socket, &packet);
}

coroutine void ws_disconnect(int socket)
{
	// Remove from event sockets
	pthread_mutex_lock(&event_sockets_mutex);
	for (int i = 0; i < arrlen(event_sockets); i++) {
		if (event_sockets[i] == socket) {
			arrdel(event_sockets, i);
			break;
		}
	}
	pthread_mutex_unlock(&event_sockets_mutex);

	// Close WebSocket
	socket = ws_detach(socket, 1000, "Normal Closure", 15, -1);
	if (socket < 0 && errno != ECONNRESET) {
		log_message(LOG_ERROR, "Error detaching WebSocket: %d - %s", errno, strerror(errno));
		return;
	}
	int rc = tcp_close(socket, -1);
	if (rc < 0) {
		log_message(LOG_ERROR, "Error closing TCP connection: %d - %s", errno, strerror(errno));
		return;
	}

	log_message(LOG_INFO, "WebSocket detached successfully");
}

void perform_ws_upgrade(int socket, char sec_websocket_key[128])
{
	// Concatenate with the GUID and hash
	const char* websocket_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char key_with_guid[256];
	snprintf(key_with_guid, sizeof(key_with_guid), "%s%s", sec_websocket_key, websocket_guid);

	unsigned char hash[SHA_DIGEST_LENGTH];
	SHA1((unsigned char*)key_with_guid, strlen(key_with_guid), hash);

	// Base64 encode hash
	char accept_key[64];
	EVP_EncodeBlock((unsigned char *)accept_key, hash, SHA_DIGEST_LENGTH);
	accept_key[strcspn(accept_key, "\n")] = '\0';

	// Send the response
	http_sendstatus(socket, 101, "Switching Protocols", -1);
	http_sendfield(socket, "Connection", "Upgrade", -1);
	http_sendfield(socket, "Upgrade", "websocket", -1);
	http_sendfield(socket, "Sec-WebSocket-Accept", accept_key, -1);
	http_done(socket, -1);
}

void render_dirlist(int socket, const char* url_path, const char* path)
{
	const char *header = 
		"<!DOCTYPE html>"
		"<html lang=\"en\" style=\"height: 100%;\">"
		"<head>"
		"<title>Directory Listing</title>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
		"</head>"
		"<body style=\"display: flex;flex-direction: column;height: 100%;\">"
		"<h1>Directory Listing</h1>"
		"<ul style=\"flex-grow: 1;\">";
	bsend(socket, header, strlen(header), -1);

	DIR* dir = opendir(path);
	if (!dir) {
		const char *error_msg = "</ul><p>Error: Unable to open directory</p></body></html>";
		bsend(socket, error_msg, strlen(error_msg), -1);
		return;
	}

	if (strlen(url_path) == 1) {
		url_path = "";
	}
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		char link[PATH_MAX];
		snprintf(link, sizeof(link), "<li><a href=\"%s/%s\">%s</a></li>", url_path, entry->d_name, entry->d_name);
		bsend(socket, link, strlen(link), -1);
	}
	closedir(dir);

	const char* back = "<li><a href=\"..\">&lt;-- Back</a></li>";
	bsend(socket, back, strlen(back), -1);
	const char *footer = "</ul>"
		"<hr style=\"width: 100%;\">"
		"<p style=\"text-align: center;\">NativeTimelapseGenerator Web Frontend | (c) Zekiah-A 2025</p>"
		"</body>"
		"</html>";
	bsend(socket, footer, strlen(footer), -1);
}

void handle_http(int socket)
{
	socket = http_attach(socket);

	char command[256];
	char resource[256];
	int rc = http_recvrequest(socket, command, sizeof(command), resource, sizeof(resource), -1);
	if (rc < 0) {
		log_message(LOG_ERROR, "Error receiving request: %d - %s", errno, strerror(errno));
		return;
	}

	// Check for ws upgrade 
	char name[256];
	char value[512]; // Problems can arise if value overflows, e.g Cookies
	char sec_websocket_key[128] = { 0 };
	bool is_websocket = false;
	while (true) {
		int rc = http_recvfield(socket, name, sizeof(name), value, sizeof(value), -1);

		if (strcasecmp(name, "Sec-WebSocket-Key") == 0) {
			strncpy(sec_websocket_key, value, sizeof(sec_websocket_key) - 1);
		}
		else if (strcasecmp(name, "Upgrade") == 0 && strcasestr(value, "websocket") != NULL) {
			is_websocket = true;
		}

		if (rc < 0 || errno == EPIPE) {
			break;
		}
	}

	if (is_websocket == true) {
		perform_ws_upgrade(socket, sec_websocket_key);
		socket = http_detach(socket, -1);
		if (socket < 0) {
			log_message(LOG_ERROR, "Error detaching HTTP: %d - %s", errno, strerror(errno));
			return;
		}
		go(ws_connect(socket));
		return;
	}

	// HTTP file request 
	http_sendstatus(socket, 200, "OK", -1);
	if (resource[0] != '/') {
		log_message(LOG_ERROR, "Invalid resource path");
		return;
	}
	if (str_ends_with(resource, "css")) {
		http_sendfield(socket, "Content-Type", "text/css", -1);
	}
	else if (str_ends_with(resource, "js")) {
		http_sendfield(socket, "Content-Type", "text/javascript", -1);
	}
	else if (str_ends_with(resource, "png")) {
		http_sendfield(socket, "Content-Type", "image/png", -1);
	}
	else {
		http_sendfield(socket, "Content-Type", "text/html", -1);
	}
	http_sendfield(socket, "Connection", "close", -1);
	socket = http_detach(socket, -1);

	const char* web_root = "./";
	char base_path[PATH_MAX];
	if (!realpath(web_root, base_path)) {
		// TODO: Fail properly
		return;
	}

	char resolved_path[PATH_MAX];
	char absolute_path[PATH_MAX];
	snprintf(absolute_path, sizeof(absolute_path), "%s%s", base_path, resource);
	if (!realpath(absolute_path, resolved_path)) {
		perror("Failed to resolve resource path");
		return;
	}

	struct stat path_stat;
	if (stat(resolved_path, &path_stat) == 0) {
		if (S_ISDIR(path_stat.st_mode)) {	
			// Render the directory listing
			render_dirlist(socket, resource, resolved_path);
			tcp_close(socket, -1);
			return;
		}
	}

	FILE* file = fopen(resolved_path, "r");
	if (file != NULL) {
		AUTOFREE char* source = NULL;
		if (fseek(file, 0L, SEEK_END) == 0) {
			long file_size = ftell(file);
			if (file_size == -1) {
				// TODO: Fail properly
				return;
			}
			source = malloc(sizeof(char) * (file_size + 1));

			// Return to start
			if (fseek(file, 0L, SEEK_SET) != 0) {
				// TODO: Fail properly
				return;
			}

			size_t source_size = fread(source, sizeof(char), file_size, file);
			if (ferror(file) != 0) {
				// TODO: Fail properly
				return;
			}
			bsend(socket, source, source_size, -1);
		}
		fclose(file);
	}
	else {
		const char* file_not_found = "Error: File not found!";
		bsend(socket, file_not_found, strlen(file_not_found), -1);
	}

	tcp_close(socket, -1);
}

void parse_command(char* input)
{
	char* saveptr = NULL;
	char* command = strtok_r(input, " ", &saveptr);
	if (!command) {
		return;
	}

	if (strcmp(command, "start_generation") == 0) {
		char* repo_url = strdup(strtok_r(NULL, " ", &saveptr));
		char* download_base_url = strdup(strtok_r(NULL, " ", &saveptr));
		char* game_server_base_url = strdup(strtok_r(NULL, " ", &saveptr));
		char* commit_hashes_file_name = strdup(strtok_r(NULL, " ", &saveptr));
		char* max_top_placers_str = strdup(strtok_r(NULL, " ", &saveptr));
		size_t max_top_placers = strtoul(max_top_placers_str, NULL, 10);

		if (repo_url && download_base_url && commit_hashes_file_name && game_server_base_url) {
			Config config = (Config) {
				repo_url,
				download_base_url,
				game_server_base_url,
				commit_hashes_file_name,
				max_top_placers
			};
			ui_start_generation(config);
		}
		else {
			log_message(LOG_ERROR, "Invalid arguments for start_generatio");
		}
	}
	else if (strcmp(command, "stop_generation") == 0) {
		ui_stop_generation();
	}
	else if (strcmp(command, "add_worker") == 0) {
		char* worker_type_str = strdup(strtok(NULL, " "));
		char* add_str = strdup(strtok(NULL, " "));
		if (worker_type_str && add_str) {
			int worker_type = atoi(worker_type_str);
			int add = atoi(add_str);
			ui_add_worker(worker_type, add);
		}
		else {
			log_message(LOG_ERROR, "Invalid arguments for add_worker");
		}
	}
	else if (strcmp(command, "remove_worker") == 0) {
		char* worker_type_str = strdup(strtok(NULL, " "));
		char* remove_str = strdup(strtok(NULL, " "));
		if (worker_type_str && remove_str) {
			int worker_type = atoi(worker_type_str);
			int remove = atoi(remove_str);
			ui_remove_worker(worker_type, remove);
		}
		else {
			log_message(LOG_ERROR, "Invalid arguments for remove_worker");
		}
	}
	else {
		log_message(LOG_ERROR, "Unknown command: %s", command);
	}
}

void handle_sigint(int _)
{
	ui_exit();
}

void* start_repl(void* _)
{
	struct sigaction sa;
	sa.sa_handler = handle_sigint;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	char* input = NULL;
	while (repl_keep_running && (input = readline("> ")) != NULL) {
		if (strlen(input) > 0) {
			add_history(input);
			parse_command(input);
		}
		free(input);
	}

	return NULL;
}

void* start_socket(void* _)
{
	log_message(LOG_INFO, "Starting web frontend...");
	struct ipaddr addr;
	ipaddr_local(&addr, NULL, 5555, 0);
	int listener = tcp_listen(&addr, 10);
	if (listener < 0) {
		log_message(LOG_ERROR, "Failed to start socket: tcp_listen failed with error %d %s", errno, strerror(errno));
		return NULL;
	}
	log_message(LOG_INFO, "Started hosting web frontend at http://localhost:5555");

	while (socket_keep_running) {
		int socket = tcp_accept(listener, NULL, -1);
		if (socket < 0) {
			log_message(LOG_ERROR, "Failed to accept tcp connection: %d %s", errno, strerror(errno));
			continue;
		}

		handle_http(socket);
	}
	return NULL;
}

void start_console()
{
	pthread_create(&repl_thread_id, NULL, start_repl, NULL);
	pthread_create(&socket_thread_id, NULL, start_socket, NULL);
}

void stop_console()
{
	log_message(LOG_INFO, "Stopping console & disconnecting ws clients...");
	for (int i =  0; i < arrlen(event_sockets); i++) {
		ws_disconnect(event_sockets[i]);
	}

	repl_keep_running = false;
	pthread_cancel(	repl_thread_id);
	socket_keep_running = false;
	pthread_cancel(socket_thread_id);
}

//__attribute__((__format__ (__printf__, 2, 0)))
void log_message(LogType type, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	size_t size = (size_t) vsnprintf(NULL, 0, format, args) + 1; // +1 for null-terminator
	va_end(args);

	char* buffer = (char*) malloc(size);
	va_start(args, format);
	vsnprintf(buffer, size, format, args);
	va_end(args);

	// Add to log history
	time_t date = time(NULL);
	LogMessage log_message = (LogMessage) {
		.type = type,
		.date = date,
		.message = buffer
	};
	arrput(log_messages, log_message);

	// Send websocket update
	bw_stackfree(packet) = bw_create_default();
	bw_u8(&packet, EVENT_PACKET_LOG_MESSAGE);
	bw_u8(&packet, type);
	bw_u64(&packet, date);
	bw_str(&packet, buffer);
	ws_send_all_packet(&packet);

	// Print to console
	if (type == LOG_INFO) {
		fprintf(stdout, "log_message: [INFO] %s\n", buffer);
	}
	else if (type == LOG_WARNING) {
		fprintf(stderr, "\x1b[33mlog_message: [WARNING] %s\x1b[0m\n", buffer);
	}
	else if (type == LOG_ERROR) {
		fprintf(stderr, "\x1b[31mlog_message: [ERROR] %s\x1b[0m\n", buffer);
	}
}

void update_worker_stats(WorkerType worker_type, int count)
{
	// TODO: Send websocket update

	AUTOFREE char* print = NULL;
	asprintf(&print, "\x1b[34mworker_stats:\x1b[0m type: %d count: %d", worker_type, count);
	puts(print);
}

typedef struct save_jobs_entry {
	SaveJobType key; // save job type
	SaveResult* value; // array of save results
} SaveJobsEntry;

void update_save_stats(int completed_saves, float saves_per_second, SaveResult* save_results)
{
	SaveJobsEntry* jobs_by_type = NULL;
	for (int i = 0; i < arrlen(save_results); i++) {
		SaveResult save = save_results[i];
		SaveJobsEntry* entry = (SaveJobsEntry*) hmgetp_null(jobs_by_type, save.save_type);

		if (!entry) {
			SaveResult* new_jobs = NULL;
			arrput(new_jobs, save);
			hmput(jobs_by_type, save.save_type, new_jobs);
		}
		else {
			arrput(entry->value, save);
		}
	}
	
	bw_stackfree(packet) = bw_create_default();
	bw_u8(&packet, EVENT_PACKET_SAVE_STATUS);
	bw_u32(&packet, (uint32_t) completed_saves);
	bw_u32(&packet, *(uint32_t*) &saves_per_second); // TODO: Properly implement float in nanobuf
	ssize_t saves_types_count = hmlen(jobs_by_type);
	bw_u8(&packet, (uint8_t) saves_types_count);
	for (int i = 0; i < saves_types_count; i++) {
		SaveJobType save_type = jobs_by_type[i].key;
		if (save_type == 0) {
			log_message(LOG_ERROR, LOG_HEADER"Failed to write save: Invalid save type: (0)");
			continue;
		}
		SaveResult* type_jobs = jobs_by_type[i].value;
		bw_u8(&packet, save_type);

		// Process jobs of this type
		int save_count = (int) arrlen(type_jobs);
		if (save_count < 0) {
			bw_u32(&packet, 0);
			continue;
		}
		bw_u32(&packet, (uint32_t) save_count);
		for (int j = 0; j < save_count; j++) {
			SaveResult job = type_jobs[j];
			bw_u32(&packet, (uint32_t) job.commit_id);
			bw_str(&packet, job.commit_hash ? job.commit_hash : "(null)");
			bw_u64(&packet, (uint64_t) job.date);
			bw_str(&packet, job.save_path ? job.save_path : "(null)");
		}
	}
	ws_send_all_packet(&packet);

	AUTOFREE char* print = NULL;
	asprintf(&print, 
		"\x1b[36msave_stats:\x1b[0m completed: \x1b[32m%d\x1b[0m saves_per_second: \x1b[33m%.2f\x1b[0m save_types_count: \x1b[34m%li\x1b[0m\x1b[36m",
		completed_saves, (double) saves_per_second, saves_types_count);
	puts(print);

	for (int i = 0; i < saves_types_count; i++) {
		SaveJobType save_type = jobs_by_type[i].key;
		SaveResult* type_jobs = jobs_by_type[i].value;
		AUTOFREE char* type_print = NULL;
		asprintf(&type_print,
			"\x1b[1;35m  → Save Type: \x1b[1;36m%d\x1b[0m\n"
			"    Jobs Processed: \x1b[1;33m%d\x1b[0m",
			save_type, (int) arrlen(type_jobs)
		);
		puts(type_print);
	}

	// Free inner arrays
	for (int i = 0; i < hmlen(jobs_by_type); i++) {
		if (jobs_by_type[i].key != 0) { // Skip empty buckets
			arrfree(jobs_by_type[i].value);
		}
	}
	// Free the hash map itself
	hmfree(jobs_by_type);
}

void update_start_status(bool started)
{
	bw_stackfree(packet) = bw_create_default();
	bw_u8(&packet, EVENT_PACKET_START_STATUS);
	bw_u8(&packet, started);
	ws_send_all_packet(&packet);
}

#include "lib/libnanobuf/buf_writer.h"
#include <linux/limits.h>
#include <pthread.h>
#include <stdarg.h>
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
#include <avcall.h>
#include <signal.h>

#include "lib/libdill/libdill.h"
#include "lib/libnanobuf/nanobuf.h"
#include "lib/stb/stb_ds.h"

#include "console.h"
#include "main_thread.h"
#include "memory_utils.h"

void (*add_funcs[3])() = { add_download_worker, add_render_worker, add_save_worker };
void (*remove_funcs[3])() = { remove_download_worker, remove_render_worker, remove_save_worker };

int* event_sockets = NULL;
typedef enum event_packet:uint8_t {
	EVENT_PACKET_LOG_MESSAGE = 0,
	EVENT_PACKET_WORKERS_INFO = 1
} EventPacket;

typedef enum control_packet:uint8_t {
	CONTROL_PACKET_START = 0,
	CONTROL_PACKET_STOP = 1,
	CONTROL_PACKET_ADD_WORKER = 2,
	CONTROL_PACKET_REMOVE_WORKER = 3
} ControlPacket;

typedef struct log_message {
	LogType type;
	time_t date;
	char* message;
} LogMessage;
LogMessage* log_messages = NULL;

// Called by UI therad, must pass back to main thread
void ui_start_generation(Config config)
{
	av_alist start_alist;
	av_start_struct(start_alist, &start_generation, Config, /*splittable*/0, &config);
	main_thread_post(start_alist);
}

// Called by UI thread, must pass back to main thread
void ui_stop_generation()
{
	log_message(LOG_INFO, "Generator CLI application halted. Application will terminate immediately");
	av_alist stop_alist;
	av_start_void(stop_alist, &stop_generation); 
	main_thread_post(stop_alist);
}

// Called by UI thread, must pass back to main thread
void ui_add_worker(int worker_type, int add)
{
	for (int i = 0; i < add; i++) {
		av_alist add_worker_alist;
		av_start_void(add_worker_alist, &add_funcs[worker_type]); 
		main_thread_post(add_worker_alist);
	}
}

// Called by UI thread, must pass back to main thread
void ui_remove_worker(int worker_type, int remove)
{
	for (int i = 0; i < remove; i++) {
		av_alist rm_worker_alist;
		av_start_void(rm_worker_alist, &add_funcs[worker_type]); 
		main_thread_post(rm_worker_alist);
	}
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
	uint8_t* data = packet->start;
	size_t packet_size = bw_size(packet);
	for (int i=0; i < arrlen(event_sockets); ++i) {
		msend(event_sockets[i], data, packet_size, -1);
	}
}

coroutine void ws_send_packet(int socket, BufWriter* packet)
{
	msend(socket, packet->start, bw_size(packet), -1);
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

	while (1) {
		// Receive a WebSocket packet
		size_t size = mrecv(socket, buf, sizeof(buf), -1);

		// Handle disconnection or errors
		if (size < 0) {
			if (errno == EPIPE) {
				log_message(LOG_INFO, "WebSocket client disconnected");
				break;
			}
			else {
				log_message(LOG_ERROR, "Error receiving WebSocket packet: %d - %s", errno, strerror(errno));
			}
			break;
		}
		if (size == 0) {
			log_message(LOG_WARNING, "Received an empty WebSocket packet");
			continue;
		}

		BufReader* packet = br_from_ptr(&buf, size, NULL);
		ControlPacket type = br_u8(packet);
		switch (type) {
			case CONTROL_PACKET_START: {
				Config config = { 0 };
				// Nullable
				char* repo_url_str = nb_to_cstr(br_str(packet));
				config.repo_url = strlen(repo_url_str) >  0 ? repo_url_str : NULL;
				
				config.download_base_url = nb_to_cstr(br_str(packet));
				config.game_server_base_url = nb_to_cstr(br_str(packet));
				
				// Nullable
				char* commit_hashes_str = nb_to_cstr(br_str(packet));
				config.commit_hashes_file_name = strlen(commit_hashes_str) > 0 ? commit_hashes_str : NULL;
				
				config.max_top_placers = br_u32(packet);
				ui_start_generation(config);
				break;
			}
			case CONTROL_PACKET_STOP: {
				ui_stop_generation();
				break;
			}
			case CONTROL_PACKET_ADD_WORKER: {
				WorkerType type = br_u8(packet);
				int count = br_u8(packet);
				ui_add_worker(type, count);
				break;
			}
			case CONTROL_PACKET_REMOVE_WORKER: {
				WorkerType type = br_u8(packet);
				int count = br_u8(packet);
				ui_remove_worker(type, count);
				break;
			}
			default: {
				log_message(LOG_WARNING, "Unknown control packet type: %d", type);
				break;
			}
		}
	}

	// Clean up when the connection is closed
	int rc = ws_detach(socket, 0, NULL, 0, -1);
	if (rc < 0) {
		log_message(LOG_ERROR, "Error detaching WebSocket: %d - %s", errno, strerror(errno));
	}
	else {
		log_message(LOG_INFO, "WebSocket detached successfully");
	}

	rc = hclose(socket);
	if (rc < 0) {
		log_message(LOG_ERROR, "Error closing WebSocket: %d - %s", errno, strerror(errno));
	}
}

coroutine void ws_connect(int socket)
{
	socket = ws_attach_server(socket, WS_BINARY | WS_NOHTTP, NULL, 0, NULL, 0, -1);
	if (socket < 0) {
		log_message(LOG_ERROR, "Error attaching WS server: %d - %s", errno, strerror(errno));
		return;
	}
	arrput(event_sockets, socket);

	// Start listening
	go(ws_listen(socket));

	// Send logs history
	log_message(LOG_INFO, "Event socket client connected!");
	for (int i = 0; i < arrlen(log_messages); i++) {
		LogMessage log_message = log_messages[i];
		BufWriter* packet = bw_create_default();
		bw_u8(packet, EVENT_PACKET_LOG_MESSAGE);
		bw_u8(packet, log_message.type);
		bw_u64(packet, log_message.date);
		bw_str(packet, log_message.message);
		ws_send_packet(socket, packet);
		bw_destroy(packet, true);
	}

	// Send all workers info
	BufWriter* packet = bw_create_default();
	bw_u8(packet, EVENT_PACKET_WORKERS_INFO);
	bw_u8(packet, 3); // workers count
	write_worker_infos(packet, WORKER_TYPE_DOWNLOAD);
	write_worker_infos(packet, WORKER_TYPE_RENDER);
	write_worker_infos(packet, WORKER_TYPE_SAVE);
	ws_send_packet(socket, packet);
	bw_destroy(packet, true);
}

coroutine void ws_disconnect(int socket)
{
	// Close WebSocket
	socket = ws_detach(socket, 1000, "Normal Closure", 15, -1);
	tcp_close(socket, -1);

	for (int i = 0; i < arrlen(event_sockets); i++) {
		if (event_sockets[i] == socket) {
			arrdel(event_sockets, i);
			break;
		}
	}
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
	char* command = strtok(input, " ");
	if (!command) {
		return;
	}

	if (strcmp(command, "start_generation") == 0) {
		char* repo_url = strtok(NULL, " ");
		char* download_base_url = strtok(NULL, " ");
		char* commit_hashes_file_name = strtok(NULL, " ");
		char* game_server_base_url = strtok(NULL, " ");
		char* max_top_placers_str = strtok(NULL, " ");
		int max_top_placers = atoi(max_top_placers_str);
		if (repo_url && download_base_url && commit_hashes_file_name && game_server_base_url) {
			Config config = (Config) {
				repo_url,
				download_base_url,
				commit_hashes_file_name,
				game_server_base_url,
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
		char* worker_type_str = strtok(NULL, " ");
		char* add_str = strtok(NULL, " ");
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
		char* worker_type_str = strtok(NULL, " ");
		char* remove_str = strtok(NULL, " ");
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

volatile sig_atomic_t repl_keep_running = 1;
void handle_sigint(int _)
{
	repl_keep_running = 0;
	log_message(LOG_INFO, "Stopping NativeTimelapseGenerator");
	stop_generation();
	stop_console();
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

bool socket_keep_running = true;
void start_socket()
{
	log_message(LOG_INFO, "Starting web frontend...");
	struct ipaddr addr;
	ipaddr_local(&addr, NULL, 5555, 0);
	int listener = tcp_listen(&addr, 10);
	if (listener < 0) {
		log_message(LOG_ERROR, "Failed to start socket: tcp_listen failed with error %d %s", errno, strerror(errno));
		return;
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
}

pthread_t repl_thread_id;
void* start_console(void* _)
{
	start_socket();
	pthread_create(&repl_thread_id, NULL, start_repl, NULL);
	return NULL;
}

void stop_console()
{
	log_message(LOG_INFO, "Stopping console & disconnecting ws clients...");
	for (int i =  0; i < arrlen(event_sockets); i++) {
		ws_disconnect(event_sockets[i]);
	}

	repl_keep_running = 0;
	pthread_cancel(	repl_thread_id);
	socket_keep_running = false;
}

void log_message(LogType type, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	size_t size = vsnprintf(NULL, 0, format, args) + 1; // +1 for null-terminator
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
	BufWriter* packet = bw_create_default();
	bw_u8(packet, EVENT_PACKET_LOG_MESSAGE);
	bw_u8(packet, type);
	bw_u64(packet, date);
	bw_str(packet, buffer);
	ws_send_all_packet(packet);
	bw_destroy(packet, true);

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
	asprintf(&print, "\x1b[34mworker_stats: type: %d count: %d\x1b[0m", worker_type, count);
	puts(print);
}

void update_backups_stats(int backups_total, float backups_per_second, CanvasInfo current_info)
{
	// TODO: Send websocket update

	AUTOFREE char* print = NULL;
	asprintf(&print, "\x1b[36mbackups_stats: generated: %d per second: %f processing: %li\x1b[0m", backups_total, backups_per_second, current_info.date);
	puts(print);
}
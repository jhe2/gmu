/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: httpd.h  Created: 111209
 *
 * Description: Simple (single-threaded) HTTP server (with websocket support)
 */
#ifndef GMU_HTTPD_H
#define GMU_HTTPD_H
#include <stdio.h>
#include "../../ringbuffer.h"
#include <arpa/inet.h>

#define bool int
#define true  1
#define false 0
#define SERVER_PORT (4680)
#define HTTP_RINGBUFFER_BUFFER_SIZE (131072)

#define VERBOSE (0)

#define MAX_CONNECTIONS              (100)
#define CONNECTION_TYPE_HTTP         (1)
#define CONNECTION_TYPE_WEBSOCKET    (2)
#define CONNECTION_TIMEOUT_HTTP      (10)
#define CONNECTION_TIMEOUT_WEBSOCKET (30)
#define CHUNK_SIZE (1300)

typedef enum ConnectionState {
	CON_HTTP_NEW, CON_HTTP_IDLE, CON_HTTP_BUSY, CON_HTTP_CLOSED,
	CON_WEBSOCKET_CONNECTING, CON_WEBSOCKET_OPEN, CON_ERROR
} ConnectionState;

typedef struct ConnectionStruct Connection;

struct ConnectionStruct {
	int             fd;
	time_t          connection_time;
	FILE           *local_file;
	size_t          total_size, remaining_bytes_to_send;
	ConnectionState state;
	char           *http_request_header;
	RingBuffer      rb_receive;
	int             authentication_okay;
	char            client_ip[INET6_ADDRSTRLEN];
	Connection     *prev, *next;
};

typedef enum HTTPCommand {
	GET, HEAD, POST, UNKNOWN
} HTTPCommand;

typedef struct HTTPD_Init_Params {
	int   local_only;
	char *webserver_root;
} HTTPD_Init_Params;

void *httpd_run_server(void *webserver_root);
void  httpd_stop_server(void);
void  httpd_send_websocket_broadcast(const char *str);

Connection *connection_init(int fd, const char *client_ip, Connection *prev);
void connection_reset_timeout(Connection *c);
int  connection_is_valid(Connection *c);
int  connection_is_local(Connection *c);
int  connection_is_authenticated(Connection *c);
int  connection_authenticate(Connection *c, const char *password);
void connection_close(Connection *c);
void connection_free_request_header(Connection *c);
int  connection_is_timed_out(Connection *c);
void connection_set_state(Connection *c, ConnectionState s);
ConnectionState connection_get_state(Connection *c);
int  connection_file_is_open(Connection *c);
int  connection_file_open(Connection *c, const char *filename);
void connection_file_close(Connection *c);
int  connection_get_number_of_bytes_to_send(Connection *c);
int  connection_file_read_chunk(Connection *c);

void gmu_http_playlist_get_info(Connection *c);
void gmu_http_playlist_get_item(int id, Connection *c);
void gmu_http_send_initial_information(Connection *c);
void gmu_http_get_current_trackinfo(Connection *c);
void gmu_http_playmode_get_info(Connection *c);
#endif

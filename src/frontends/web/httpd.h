/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: httpd.h  Created: 111209
 *
 * Description: Simple (single-threaded) HTTP server (with websocket support)
 */
#ifndef GMU_HTTPD_H
#define GMU_HTTPD_H
#include <stdio.h>
#include "../../ringbuffer.h"

#define bool int
#define true  1
#define false 0
#define SERVER_PORT (4680)
#define BUFFER_SIZE (2048)

#define VERBOSE (0)

#define MAX_CONNECTIONS              (100)
#define CONNECTION_TYPE_HTTP         (1)
#define CONNECTION_TYPE_WEBSOCKET    (2)
#define CONNECTION_TIMEOUT_HTTP      (10)
#define CONNECTION_TIMEOUT_WEBSOCKET (30)
#define CHUNK_SIZE (1300)

typedef enum ConnectionState {
	HTTP_NEW, HTTP_IDLE, HTTP_BUSY, HTTP_CLOSED,
	WEBSOCKET_CONNECTING, WEBSOCKET_OPEN
} ConnectionState;

typedef struct Connection {
	int             fd;
	time_t          connection_time;
	FILE           *local_file;
	int             total_size, remaining_bytes_to_send;
	ConnectionState state;
	char           *http_request_header;
	RingBuffer      rb_receive;
	char           *password_ref;
	int             authentication_okay;
} Connection;

typedef enum HTTPCommand {
	GET, HEAD, POST, UNKNOWN
} HTTPCommand;

typedef struct HTTPD_Init_Params {
	int   local_only;
	char *webserver_root;
} HTTPD_Init_Params;

void *httpd_run_server(void *webserver_root);
void  httpd_stop_server(void);
void  httpd_send_websocket_broadcast(char *str);

int  connection_init(Connection *c, int fd);
void connection_reset_timeout(Connection *c);
int  connection_is_valid(Connection *c);
int  connection_is_authenticated(Connection *c);
int  connection_authenticate(Connection *c, char *password);
void connection_close(Connection *c);
void connection_free_request_header(Connection *c);
int  connection_is_timed_out(Connection *c);
void connection_set_state(Connection *c, ConnectionState s);
ConnectionState connection_get_state(Connection *c);
int  connection_file_is_open(Connection *c);
int  connection_file_open(Connection *c, char *filename);
void connection_file_close(Connection *c);
int  connection_get_number_of_bytes_to_send(Connection *c);
int  connection_file_read_chunk(Connection *c);

void gmu_http_playlist_get_info(Connection *c);
void gmu_http_playlist_get_item(int id, Connection *c);
void gmu_http_send_initial_information(Connection *c);
void gmu_http_get_current_trackinfo(Connection *c);
void gmu_http_playmode_get_info(Connection *c);
#endif

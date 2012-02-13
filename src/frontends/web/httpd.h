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

#define bool int
#define true  1
#define false 0
#define SERVER_PORT (4680)
#define BUFFER_SIZE (2048)

#define VERBOSE (0)

#define MAX_CONNECTIONS              (100)
#define CONNECTION_TYPE_HTTP         (1)
#define CONNECTION_TYPE_WEBSOCKET    (2)
#define CONNECTION_TIMEOUT_HTTP      (5)
#define CONNECTION_TIMEOUT_WEBSOCKET (30)
#define CHUNK_SIZE (4000)

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
} Connection;

typedef enum HTTPCommand {
	GET, HEAD, POST, UNKNOWN
} HTTPCommand;

void *httpd_run_server(void *webserver_root);
void  httpd_stop_server(void);
void  httpd_send_websocket_broadcast(char *str);
#endif

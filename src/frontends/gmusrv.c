/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmusrv.c  Created: 100920
 *
 * Description: Gmu Socket Control
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../gmufrontend.h"
#include "../core.h"

static pthread_t fe_thread;

#define BUF 1024
#define PORT 4680

const char *get_name(void)
{
	return "Gmu Srv v1";
}

void shut_down(void)
{
	/* Send quit request to Socket, so the loop can terminate... */
	int                sock;
	char              *buffer = malloc(BUF);
	ssize_t            size;
	struct sockaddr_in address;
	const int          y = 1;

	printf("gmusrv: Shutting down.\n");

	if (buffer && (sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
		address.sin_family = AF_INET;
		inet_aton("127.0.0.1", &address.sin_addr);
		address.sin_port = htons(PORT);
		
		if (connect(sock, (struct sockaddr *)&address, sizeof(address)) == 0) {
			char *str = "GmuSrv1\n";
			printf("Connected to server (%s).\n", inet_ntoa(address.sin_addr));
			/* Verify server identification string... */
			size = recv(sock, buffer, BUF-1, 0);
			if (size > 0 && strncmp(buffer, str, strlen(str)) == 0) { /* okay */
				str = "_gmusrv_quit";
				if (size > 0) buffer[size] = '\0';
				/* Send _quit command to server... */
				send(sock, str, strlen(str), 0);
			}
		}
		close(sock);
	}
	if (buffer) free(buffer);

	pthread_join(fe_thread, NULL);
}

static void *thread_func(void *arg)
{
	int                sock, client_sock, res = EXIT_FAILURE;
	socklen_t          addrlen;
	char              *buffer = malloc(BUF);
	ssize_t            size;
	struct sockaddr_in address;
	const int          y = 1;

	if (buffer && (sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
		printf("gmusrv: Socket created.\n");
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(PORT);
		if (bind(sock, (struct sockaddr *)&address, sizeof(address)) == 0) {
			int running = 1;

			listen(sock, 5);
			addrlen = sizeof(struct sockaddr_in);
			
			while (running) {
				client_sock = accept(sock, (struct sockaddr *)&address, &addrlen);
				if (client_sock > 0) {
					char *str = "GmuSrv1\n";
					printf("gmusrv: Client connected (%s).\n", inet_ntoa(address.sin_addr));
					/* Send server identification string... */
					send(client_sock, str, strlen(str), 0);
					/* Receive command from client... */
					size = recv(client_sock, buffer, BUF-1, 0);
					if (size > 0) buffer[size] = '\0';
					printf("gmusrv: Command received: \"%s\"\n", buffer);
					if (strncmp(buffer, "_gmusrv_quit", 4) == 0) {
						running = 0;
					}
					if (strncmp(buffer, "playpause", 9) == 0) {
						str = "Starting playback.\n";
						send(client_sock, str, strlen(str), 0);
						gmu_core_play();
					}
					if (strncmp(buffer, "next", 4) == 0) {
						str = "Jumping to next track in playlist.\n";
						send(client_sock, str, strlen(str), 0);
						gmu_core_next();
					}
					if (strncmp(buffer, "prev", 4) == 0) {
						str = "Jumping to previous track in playlist.\n";
						send(client_sock, str, strlen(str), 0);
						gmu_core_previous();
					}
					if (strncmp(buffer, "quit", 4) == 0) {
						str = "Terminating Gmu.\n";
						send(client_sock, str, strlen(str), 0);
						gmu_core_quit();
					}
					switch (buffer[0]) {
						case 's': /* status */
							
							break;
						case 't': { /* track info */
							TrackInfo *ti = gmu_core_get_current_trackinfo_ref();
							char       ti_str[320];
							
							snprintf(ti_str, 319, "%s - %s", trackinfo_get_artist(ti), trackinfo_get_title(ti));
							send(client_sock, ti_str, strlen(ti_str), 0);
							break;
						}
						default:
							break;
					}
				}
				close(client_sock);
			}
			close(sock);
			res = EXIT_SUCCESS;
		} else {
			printf("gmusrv: ERROR: Port %d unavailable.\n", PORT);
		}
	}
	if (buffer) free(buffer);
	return NULL;
}

void init(void)
{
	pthread_create(&fe_thread, NULL, thread_func, NULL);
}

static GmuFrontend gf = {
	"gmusrv",
	get_name,
	init,
	shut_down,
	NULL,
	NULL
};

GmuFrontend *gmu_register_frontend(void)
{
	return &gf;
}

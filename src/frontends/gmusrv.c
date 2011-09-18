/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
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
#include <fcntl.h>
#include <pthread.h>
#include "../gmufrontend.h"
#include "../core.h"
#include "../debug.h"

static pthread_t fe_thread;
static int       running;

#define BUF 1024
#define PORT 4680

static const char *get_name(void)
{
	return "Gmu Srv v1";
}

static void shut_down(void)
{
	wdprintf(V_INFO, "gmusrv", "Shutting down.\n");
	running = 0;
	pthread_join(fe_thread, NULL);
}

static void *thread_func(void *arg)
{
	int                sock, client_sock;
	socklen_t          addrlen;
	char              *buffer = malloc(BUF);
	ssize_t            size;
	struct sockaddr_in address;
	const int          y = 1;
	ConfigFile        *config = gmu_core_get_config();
	char              *password = cfg_get_key_value(*config, "gmusrv.Password");

	if (buffer && (sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
		wdprintf(V_INFO, "gmusrv", "Socket created.\n");
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
		fcntl(sock, F_SETFL, O_NONBLOCK);
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(PORT);
		if (bind(sock, (struct sockaddr *)&address, sizeof(address)) == 0) {
			listen(sock, 5);
			addrlen = sizeof(struct sockaddr_in);

			running = 1;
			while (running) {
				usleep(10000);
				client_sock = accept(sock, (struct sockaddr *)&address, &addrlen);
				if (client_sock > 0) {
					int   auth_okay = 0;
					char *str = "GmuSrv1\n";
					wdprintf(V_INFO, "gmusrv", "Client connected (%s).\n", inet_ntoa(address.sin_addr));
					/* Send server identification string... */
					send(client_sock, str, strlen(str), 0);
					/* Receive password from client... */
					size = recv(client_sock, buffer, BUF-1, 0);
					if (size > 0) {
						char tmp[4];
						buffer[size] = '\0';
						if (password && strncmp(buffer, password, size) == 0)
							auth_okay = 1;
						else
							auth_okay = 0;
						snprintf(tmp, 3, "%d", auth_okay);
						send(client_sock, tmp, strlen(tmp), 0);
					}
					if (auth_okay) {
						/* Receive command from client... */
						size = recv(client_sock, buffer, BUF-1, 0);
						if (size > 0) buffer[size] = '\0';
						wdprintf(V_DEBUG, "gmusrv", "Command received: \"%s\"\n", buffer);
						if (strncmp(buffer, "_gmusrv_quit", 4) == 0) {
							running = 0;
						}
						switch (buffer[0]) {
							case 'p':
								str = "GmuSrv: Play/pause\n";
								send(client_sock, str, strlen(str), 0);
								gmu_core_play();
								break;
							case 'n':
								str = "GmuSrv: Jumping to next track in playlist.\n";
								send(client_sock, str, strlen(str), 0);
								gmu_core_next();
								break;							
							case 'l':
								str = "GmuSrv: Jumping to previous track in playlist.\n";
								send(client_sock, str, strlen(str), 0);
								gmu_core_previous();
								break;
							case 'x':
								str = "GmuSrv: Terminating Gmu.\n";
								send(client_sock, str, strlen(str), 0);
								gmu_core_quit();
								break;
							case 's': /* status */
								
								break;
							case 't': { /* track info */
								TrackInfo *ti = gmu_core_get_current_trackinfo_ref();
								char       ti_str[320];

								snprintf(ti_str, 319, "%s - %s", trackinfo_get_artist(ti), trackinfo_get_title(ti));
								send(client_sock, ti_str, strlen(ti_str), 0);
								break;
							}
							case 'h':
								str = "GmuSrv help\nAvailable commands:\n"
								      "p : Play/Pause\n"
								      "n : Jump to next track in playlist\n"
								      "l : Jump to previous track in playlist\n"
								      "x : Terminate Gmu\n"
								      "t : Track info\n";
								send(client_sock, str, strlen(str), 0);
								break;
							default:
								str = "GmuSrv: Unknown command\n";
								send(client_sock, str, strlen(str), 0);
								break;
						}
					} else {
						wdprintf(V_WARNING, "gmusrv", "Client authentication failed.\n");
					}
					close(client_sock);
				}
			}
			close(sock);
		} else {
			wdprintf(V_ERROR, "gmusrv", "ERROR: Port %d unavailable.\n", PORT);
		}
	}
	if (buffer) free(buffer);
	return NULL;
}

static int init(void)
{
	int res = 0;
	if (pthread_create(&fe_thread, NULL, thread_func, NULL) == 0)
		res = 1;
	return res;
}

static GmuFrontend gf = {
	"gmusrv",
	get_name,
	init,
	shut_down,
	NULL,
	NULL,
	NULL
};

GmuFrontend *gmu_register_frontend(void)
{
	return &gf;
}

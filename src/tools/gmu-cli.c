/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmu-cli.c  Created: 100920
 *
 * Description: Gmu command line tool
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

#define BUF 1024
#define PORT 4680

int main(int argc, char **argv)
{
	int                sock, res = EXIT_FAILURE;
	char              *buffer = malloc(BUF);
	ssize_t            size;
	struct sockaddr_in address;
	const int          y = 1;

	if (argc >= 2) {
		if (buffer && (sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
			address.sin_family = AF_INET;
			inet_aton("127.0.0.1", &address.sin_addr);
			address.sin_port = htons(PORT);
			
			if (connect(sock, (struct sockaddr *)&address, sizeof(address)) == 0) {
				char *str = "GmuSrv1\n", *password = "password";
				/*printf("Connected to server (%s).\n", inet_ntoa(address.sin_addr));*/
				/* Verify server identification string... */
				size = recv(sock, buffer, BUF-1, 0);
				if (size > 0 && strncmp(buffer, str, strlen(str)) == 0) { /* okay */
					if (size > 0) buffer[size] = '\0';
					/* Send password to server... */
					send(sock, password, strlen(password), 0);
					/* Receive response... */
					size = recv(sock, buffer, BUF-1, 0);
					if (size > 0 && buffer[0] == '1') {
						printf("Password okay.\n");
						/* Send command to server... */
						send(sock, argv[1], strlen(argv[1]), 0);
						/* Receive response... */
						size = recv(sock, buffer, BUF-1, 0);
						if (size > 0) printf("%s", buffer);
					} else {
						printf("Invalid password.\n");
					}
				}
			}
			close(sock);
			res = EXIT_SUCCESS;
		}
	} else {
		printf("Usage %s <command> [parameters]\n", argv[0]);
		printf("Commands:\n- p\n- j [tracknr]\n- n\n- l\n- t\n");
	}
	if (buffer) free(buffer);
	return res;
}

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
#include <sys/stat.h>
#include <sys/types.h>
#include "../wejpconfig.h"

#define BUF 1024
#define PORT 4680

int main(int argc, char **argv)
{
	int                sock, res = EXIT_FAILURE;
	char              *buffer = malloc(BUF);
	ssize_t            size;
	struct sockaddr_in address;
	const int          y = 1;
	ConfigFile         config;
	char              *password, *host;
	char               config_file_path[256], *homedir;

	cfg_init_config_file_struct(&config);
	cfg_add_key(&config, "Host", "127.0.0.1");
	cfg_add_key(&config, "Password", "stupidpassword");

	homedir = getenv("HOME");
	if (homedir) {
		snprintf(config_file_path, 255, "%s/.config/gmu/gmu-cli.conf", homedir);
		if (cfg_read_config_file(&config, config_file_path) != 0) {
			char tmp[256];
			printf("gmu-cli: No config file found. Creating a config file at %s. Please edit that file and try again.\n", config_file_path);
			snprintf(tmp, 255, "%s/.config", homedir);
			mkdir(tmp, 0);
			snprintf(tmp, 255, "%s/.config/gmu", homedir);
			mkdir(tmp, 0);
			if (cfg_write_config_file(&config, config_file_path))
				printf("gmu-cli: ERROR: Unable to create config file.\n");
			cfg_free_config_file_struct(&config);
			exit(2);
		}
		host = cfg_get_key_value(config, "Host");
		password = cfg_get_key_value(config, "Password");
		if (!host || !password) {
			printf("gmu-cli: ERROR: Invalid configuration.\n");
			exit(4);
		}
	} else {
		printf("gmu-cli: ERROR: Cannot find user's home directory.\n");
		exit(3);
	}

	if (argc >= 2) {
		if (buffer && (sock = socket(AF_INET, SOCK_STREAM, 0)) > 0) {
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
			address.sin_family = AF_INET;
			inet_aton(host, &address.sin_addr);
			address.sin_port = htons(PORT);
			
			if (connect(sock, (struct sockaddr *)&address, sizeof(address)) == 0) {
				char *str = "GmuSrv1\n";
				/*printf("Connected to server (%s).\n", inet_ntoa(address.sin_addr));*/
				/* Verify server identification string... */
				size = recv(sock, buffer, BUF-1, 0);
				if (size > 0 && strncmp(buffer, str, strlen(str)) == 0) { /* okay */
					if (size > 0) buffer[size] = '\0';
					send(sock, password, strlen(password), 0);
					size = recv(sock, buffer, BUF-1, 0);
					if (size > 0 && buffer[0] == '1') {
						send(sock, argv[1], strlen(argv[1]), 0);
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
		printf("Usage: %s <command> [parameters]\n", argv[0]);
		printf("Use \"%s h\" for a list of available commands.\n", argv[0]);
	}
	cfg_free_config_file_struct(&config);
	if (buffer) free(buffer);
	return res;
}

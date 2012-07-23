/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: lirc.c  Created: 100502
 *
 * Description: Gmu log bot client (for logging played tracks)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <lirc/lirc_client.h>
#include "../gmufrontend.h"
#include "../core.h"
#include "../debug.h"

static pthread_t fe_thread;

static const char *get_name(void)
{
	return "Gmu LIRC Remote Control Frontend v0.1";
}

static int run = 1;

static void shut_down(void)
{
	wdprintf(V_DEBUG, "lirc_frontend", "Shutting down.\n");
	run = 0;
	pthread_join(fe_thread, NULL);
	wdprintf(V_INFO, "lirc_frontend", "All done.\n");
}

static void *thread_func(void *arg)
{
	struct lirc_config *config;
	int    sock, flags;

	if ((sock = lirc_init("gmu", 1)) != -1) {
		fcntl(sock, F_SETOWN, getpid());
		flags = fcntl(sock, F_GETFL, 0);
		if (flags != -1) fcntl(sock, F_SETFL, flags|O_NONBLOCK);
		if (lirc_readconfig(NULL, &config, NULL) == 0) {
			char *code, *c;
			int   ret = 0;

			while (run && lirc_nextcode(&code) == 0) {
				if (code != NULL) {
					if ((ret = lirc_code2char(config, code, &c)) == 0 && c != NULL) {
						wdprintf(V_DEBUG, "lirc_frontend", "Got button press.\n");
						if (strcmp(c, "play") == 0)
							gmu_core_play();					
						else if (strcmp(c, "pause") == 0)
							gmu_core_pause();
						else if (strcmp(c, "stop") == 0)
							gmu_core_stop();
						else if (strcmp(c, "toggle_pause") == 0)
							gmu_core_play_pause();
						else if (strcmp(c, "toggle_play_pause") == 0)
							gmu_core_play_pause();
						else if (strcmp(c, "next") == 0)
							gmu_core_next();				
						else if (strcmp(c, "prev") == 0)
							gmu_core_previous();
						else if (strcmp(c, "volume_up") == 0)
							gmu_core_set_volume(gmu_core_get_volume()+1);
						else if (strcmp(c, "volume_down") == 0)
							gmu_core_set_volume(gmu_core_get_volume()-1);
						else
							wdprintf(V_WARNING, "lirc_frontend", "Unknown command: %s\n", c);
						c = NULL;
					}
				}
				usleep(400);
				free(code);
				if (ret == -1) break;
			}
			wdprintf(V_INFO, "lirc_frontend", "Exitting.\n");
			lirc_freeconfig(config);
		}
		lirc_deinit();
	}
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
	"lirc_frontend",
	get_name,
	init,
	shut_down,
	NULL,
	NULL
};

GmuFrontend *GMU_REGISTER_FRONTEND(void)
{
	return &gf;
}

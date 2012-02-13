/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmuhttp.c  Created: 120118
 *
 * Description: Gmu web frontend
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdint.h>
#include <pthread.h>
#include "../../core.h"
#include "../../gmufrontend.h"
#include "httpd.h"

static const char *get_name(void)
{
	return "Gmu HTTP server frontend v0.1";
}

static pthread_t fe_thread;

static void server_stop(void)
{
	httpd_stop_server();
	pthread_join(fe_thread, NULL);
}

static int init(void)
{
	int   res = 0;
	char *webserver_root = gmu_core_get_base_dir();

	if (pthread_create(&fe_thread, NULL, httpd_run_server, webserver_root) == 0)
		res = 1;
	return res;
}

static int event_callback(GmuEvent event)
{
	switch (event) {
		case GMU_QUIT:
			break;
		case GMU_TRACKINFO_CHANGE:
			httpd_send_websocket_broadcast(
				"{ \"cmd\": \"trackinfo\", \"title\" : \"unknown\", \"artist\" : \"unknown\" }"
			);
			break;
		case GMU_PLAYBACK_STATE_CHANGE: {
			char str[256];
			snprintf(str, 255, "{ \"cmd\": \"playback_state\", \"state\" : %d }", gmu_core_get_status());
			httpd_send_websocket_broadcast(str);
			break;
		}
		default:
			break;
	}
	return 0;
}

static GmuFrontend gf = {
	"web_frontend",
	get_name,
	init,
	server_stop,
	NULL,
	event_callback,
	NULL
};

GmuFrontend *gmu_register_frontend(void)
{
	return &gf;
}

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
#include <pthread.h>
#include "../../gmufrontend.h"
#include "httpd.h"

static const char *get_name(void)
{
	return "Gmu HTTP server frontend v0.1";
}

static pthread_t fe_thread;
static int server_running = 0;

static void server_stop(void)
{
	server_running = 0;
	pthread_join(fe_thread, NULL);
}

static int init(void)
{
	int res = 0;
	if (pthread_create(&fe_thread, NULL, httpd_run_server, NULL) == 0)
		res = 1;
	return res;
}

static GmuFrontend gf = {
	"web_frontend",
	get_name,
	init,
	server_stop,
	NULL
};

GmuFrontend *gmu_register_frontend(void)
{
	return &gf;
}

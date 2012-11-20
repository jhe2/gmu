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
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "../../core.h"
#include "../../trackinfo.h"
#include "../../gmufrontend.h"
#include "httpd.h"
#include "json.h"

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

#define MSG_MAX_LEN 512

static int event_callback(GmuEvent event, int param)
{
	char msg[MSG_MAX_LEN];
	int  r;

	switch (event) {
		case GMU_QUIT:
			break;
		case GMU_TRACKINFO_CHANGE: {
			TrackInfo *ti = gmu_core_get_current_trackinfo_ref();
			char *tmp_title, *tmp_artist, *tmp_album;
			
			tmp_title  = json_string_escape_alloc(trackinfo_get_title(ti));
			tmp_artist = json_string_escape_alloc(trackinfo_get_artist(ti));
			tmp_album  = json_string_escape_alloc(trackinfo_get_album(ti));
			r = snprintf(msg, MSG_MAX_LEN,
			             "{ \"cmd\": \"trackinfo\", \"title\" : \"%s\", \"artist\" : \"%s\", \"album\" : \"%s\" }",
			             tmp_title, tmp_artist, tmp_album);
			if (r < MSG_MAX_LEN && r > 0) httpd_send_websocket_broadcast(msg);
			if (tmp_title)  free(tmp_title);
			if (tmp_artist) free(tmp_artist);
			if (tmp_album)  free(tmp_album);
			break;
		}
		case GMU_PLAYBACK_STATE_CHANGE: {
			r = snprintf(msg, MSG_MAX_LEN,
			             "{ \"cmd\": \"playback_state\", \"state\" : %d }",
			             gmu_core_get_status());
			if (r < MSG_MAX_LEN && r > 0) httpd_send_websocket_broadcast(msg);
			break;
		}
		case GMU_PLAYLIST_CHANGE: {
			r = snprintf(msg, MSG_MAX_LEN,
			             "{ \"cmd\": \"playlist_change\", \"changed_at_position\" : 0, \"length\" : %d }",
			             gmu_core_playlist_get_length());
			if (r < MSG_MAX_LEN && r > 0) httpd_send_websocket_broadcast(msg);
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

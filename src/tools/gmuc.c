/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmuc.c  Created: 120920
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
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <curses.h>
#include <signal.h>
#include <wctype.h>
#include <limits.h>
#include "../wejconfig.h"
#include "../ringbuffer.h"
#include "../debug.h"
#include "../frontends/web/websocket.h"
#include "../frontends/web/json.h"
#include "../dir.h"
#include "window.h"
#include "listwidget.h"
#include "ui.h"
#include "../nethelper.h"
#include "../util.h"

#define BUF 1024
#define PORT 4680

/* Number of idle loop cycles until a PING request will be send */
#define PING_TIMEOUT 30
/* Maximum number of loop cycles until the connection times out after a PING */
#define PONG_TIMEOUT 5

typedef enum { STATE_WEBSOCKET_HANDSHAKE, STATE_CONNECTION_ESTABLISHED, STATE_WEBSOCKET_HANDSHAKE_FAILED } State;

static volatile sig_atomic_t resized = 0;

static void sig_handler_sigwinch(int sig)
{
	resized = 1;
}

typedef enum Command {
	PLAY,
	PAUSE,
	NEXT,
	PREVIOUS,
	STOP,
	FILES,
	ADD,
	SEARCH,
	PING,
	RAW, /* Sends raw data as command to thr server (which has to be supplied as parameter) */
	NO_COMMAND
} Command;

static char *cmd_arr[] = {
	"play",
	"pause",
	"next",
	"previous",
	"stop",
	"files",
	"add",
	"search",
	"raw",
	NULL
};

static void set_terminal_title(char *title)
{
	char *env_term = getenv("TERM");
	if (!title) title = "Terminal";
	if ((strncmp(env_term, "xterm", 5) == 0) ||
	    (strncmp(env_term, "rxvt", 4) == 0)  ||
	    (strcmp(env_term, "Eterm") == 0)     ||
	    (strcmp(env_term, "aixterm") == 0)   ||
	    (strcmp(env_term, "dtterm") == 0)    ||
	    (strcmp(env_term, "iris-ansi") == 0)) {
		printf("\33]0;%s\7", title);
	} else if (strncmp(env_term, "screen", 6) == 0) {
		printf("\033k%s\033\\", title);
		printf("\33]0;%s\7", title);
	}
}

/**
 * Parses the input string and stores the detected command in cmd and 
 * its parameters in params when successful and returns 1. Memory for
 * 'params' will be allocated as needed. If there are no parameters
 * 'params' will be set to NULL. No memory will be allocated for 'cmd'.
 * When unable to parse the input, the function returns 0.
 */
static int parse_input_alloc(char *input, Command *cmd, char **params)
{
	int res = 0;
	if (input) {
		int   len = strlen(input);
		char *input_copy;
		if (len > 0) {
			input_copy = malloc(len+1);
			if (input_copy) {
				int   i;
				char *parameters = NULL, *command = NULL;
				memcpy(input_copy, input, len+1);
				for (i = 0; input_copy[i] != ' ' && input_copy[i] != '\0'; i++);
				if (input_copy[i] == ' ') {
					input_copy[i] = '\0';
					parameters = input_copy+i+1;
				}
				command = input_copy;
				/* Check command */
				for (i = 0; cmd_arr[i]; i++) {
					if (strncmp(command, cmd_arr[i], strlen(command)) == 0) {
						int len_params = parameters ? strlen(parameters) : 0;

						*cmd = i;
						if (len_params > 0) {
							*params = malloc(len_params+1);
							if (*params) {
								memcpy(*params, parameters, len_params+1);
							}
						} else {
							params = NULL;
						}
						res = 1;
						break;
					}
				}
				free(input_copy);
			}
		}
	}
	return res;
}

static char *wchars_to_utf8_str_realloc(char *input, wchar_t *wchars)
{
	size_t len = wcstombs(NULL, wchars, 0);
	if (len != (size_t) -1) {
		input = realloc(input, len + 1);
		if (input) {
			if (wcstombs(input, wchars, len) != len) {
				free(input);
				input = NULL;
			} else {
				input[len] = '\0';
			}
		}
	}
	return input;
}

static State do_websocket_handshake(RingBuffer *rb, State state)
{
	int  i, data_available = 1;
	char buf[256], ch;

	memset(buf, 0, 256);
	ringbuffer_set_unread_pos(rb);
	while (data_available && state == STATE_WEBSOCKET_HANDSHAKE) {
		for (i = 0, ch = 0; i < 255 && ch != '\n'; ) {
			if (ringbuffer_read(rb, &ch, 1)) {
				buf[i] = ch;
				if (buf[i] == '\r') buf[i] = '\0';
				i++;
			} else {
				ringbuffer_unread(rb);
				data_available = 0;
				break;
			}
		}
		if (buf[0] == '\n' || (buf[0] == '\0' && buf[1] == '\n')) { /* End of header */
			/* Check for valid websocket server response */
			wdprintf(V_DEBUG, "gmuc", "End of header detected!\n");
			if (1) /* temporary */
				state = STATE_CONNECTION_ESTABLISHED;
			else
				state = STATE_WEBSOCKET_HANDSHAKE_FAILED;
		}
		/*if (buf[i] == '\r' || buf[i] == '\n') buf[i] = '\0';*/
		buf[i] = '\0';
		if (i > 0) wdprintf(V_DEBUG, "gmuc", "i=%d LINE=[%s]\n", i, buf);
	}
	return state;
}

static void cmd_playback_time_change(UI *ui, JSON_Object *json)
{
	ui_update_playback_time(ui, (int)json_get_number_value_for_key(json, "time"));
	ui_draw_header(ui);
}

static void cmd_track_change(UI *ui, JSON_Object *json)
{
	int ppos = (int)json_get_number_value_for_key(json, "playlist_pos");
	ui_update_playlist_pos(ui, ppos);
}

static void cmd_trackinfo(UI *ui, JSON_Object *json)
{
	char *artist  = json_get_string_value_for_key(json, "artist");
	char *title   = json_get_string_value_for_key(json, "title");
	char *album   = json_get_string_value_for_key(json, "album");
	char *date    = json_get_string_value_for_key(json, "date");
	int   minutes = (int)json_get_number_value_for_key(json, "length_min");
	int   seconds = (int)json_get_number_value_for_key(json, "length_sec");
	seconds += minutes * 60;
	ui_update_trackinfo(ui, title, artist, album, date);
	ui_set_total_track_time(ui, seconds);
	ui_draw_header(ui);
	ui_refresh_active_window(ui);
}

static void cmd_playlist_change(UI *ui, JSON_Object *json, int sock)
{
	char title[64];
	int  length     = (int)json_get_number_value_for_key(json, "length");
	int  changed_at = (int)json_get_number_value_for_key(json, "changed_at_position");
	wprintw(ui->win_cmd->win, "Playlist has been changed!\n");
	wprintw(ui->win_cmd->win, "Length=%d Pos=%d\n", length, changed_at);
	if (length < 0) length = 0;
	listwidget_set_length(ui->lw_pl, length);
	snprintf(title, 64, "Playlist (%d)", length);
	window_update_title(ui->lw_pl->win, title);
	ui_refresh_active_window(ui);
	if (listwidget_get_selection(ui->lw_pl) > changed_at)
		listwidget_set_cursor(ui->lw_pl, changed_at);
	if (length > 0 && changed_at >= 0) {
		char str[64];
		snprintf(str, 63, "{\"cmd\":\"playlist_get_item\", \"item\":%d}", changed_at);
		if (!websocket_send_str(sock, str, 1))
			wprintw(ui->win_cmd->win, "FAILED sending message over socket!\n");
		else
			wprintw(ui->win_cmd->win, "Message sent SUCCESSFULLY.\n");
		ui_refresh_active_window(ui);
	}
}

static void cmd_playlist_item(UI *ui, JSON_Object *json, int sock)
{
	char  str[128];
	char *title = json_get_string_value_for_key(json, "title");
	int   pos = (int)json_get_number_value_for_key(json, "position");
	if (pos >= 0 && title) {
		snprintf(str, 127, "%5d", pos+1);
		listwidget_set_cell_data(ui->lw_pl, pos, 0, str);
		listwidget_set_cell_data(ui->lw_pl, pos, 1, title);
		if (pos >= ui->lw_pl->first_visible_row &&
		    pos < ui->lw_pl->first_visible_row + ui->lw_pl->win->height-2)
			ui_refresh_active_window(ui);
		if (pos+1 < listwidget_get_rows(ui->lw_pl)) {
			snprintf(str, 127, "{\"cmd\":\"playlist_get_item\", \"item\":%d}", pos+1);
			websocket_send_str(sock, str, 1);
		}
	}
}

/* Returns a pointer to a newly allocated string containing the 
 * actually read directory */
static char *cmd_dir_read(UI *ui, JSON_Object *json)
{
	int          i, start;
	JSON_Key    *jk = json_get_key_object_for_key(json, "data");
	JSON_Object *jo = jk ? jk->key_value_object : NULL;
	const char  *tmp = json_get_string_value_for_key(json, "path");
	const char  *json_res = json_get_string_value_for_key(json, "res");
	char        *cur_dir = NULL;

	if (strcmp("ok", json_res) == 0) {
		if (tmp) {
			int size = strlen(tmp)+1;
			cur_dir = malloc(size);
			memcpy(cur_dir, tmp, size);
		}
		tmp = NULL;

		if (jo) tmp = json_get_first_key_string(jo);
		start = -1;
		if (tmp) start = atoi(tmp);
		if (!jk) wprintw(ui->win_cmd->win, "ERROR: No key for 'data'.\n");

		if (start == 0) {
			listwidget_clear_all_rows(ui->lw_fb);
			ui_refresh_active_window(ui);
		}
		if (start >= 0) {
			for (i = start; ; i++) {
				JSON_Object *j_file;
				JSON_Key    *jk;
				char tmp[16];
				snprintf(tmp, 15, "%d", i);
				if (!jo) wprintw(ui->win_cmd->win, "ERROR: No object.\n");
				jk = json_get_key_object_for_key(jo, tmp);
				j_file = jk ? jk->key_value_object : NULL;
				if (j_file) {
					char *jv     = json_get_string_value_for_key(j_file, "name");
					int   is_dir = json_get_number_value_for_key(j_file, "is_dir");
					int   fsize  = json_get_number_value_for_key(j_file, "size");
					if (!jv) break;
					listwidget_add_row(ui->lw_fb);
					listwidget_set_cell_data(ui->lw_fb, i, 1, jv);
					if (!is_dir)
						snprintf(tmp, 15, "%d", fsize / 1024);
					else
						snprintf(tmp, 15, "[DIR]");
					listwidget_set_cell_data(ui->lw_fb, i, 0, tmp);
				} else {
					break;
				}
			}
			ui_refresh_active_window(ui);
		}
	}
	return cur_dir;
}

static void cmd_mlib_result(UI *ui, JSON_Object *json)
{
	int pos = (int)json_get_number_value_for_key(json, "pos");

	if (pos == 0) listwidget_clear_all_rows(ui->lw_mlib_search);
	if (pos >= 0) {
		int   row;
		int   id     = json_get_number_value_for_key(json, "id");
		char *artist = json_get_string_value_for_key(json, "artist");
		char *album  = json_get_string_value_for_key(json, "album");
		char *title  = json_get_string_value_for_key(json, "title");
		char  tmpid[8];

		snprintf(tmpid, 8, "%d", id);
		row = listwidget_add_row(ui->lw_mlib_search) - 1;
		listwidget_set_cell_data(ui->lw_mlib_search, row, 0, artist);
		listwidget_set_cell_data(ui->lw_mlib_search, row, 1, album);
		listwidget_set_cell_data(ui->lw_mlib_search, row, 2, title);
		listwidget_set_cell_data(ui->lw_mlib_search, row, 3, tmpid);
		ui_refresh_active_window(ui);
	}
}

static void cmd_mlib_browse_result(UI *ui, JSON_Object *json)
{
	int pos = (int)json_get_number_value_for_key(json, "pos");

	if (pos >= 0) {
		int   row;
		/*int   id     = json_get_number_value_for_key(json, "id");*/
		char *artist = json_get_string_value_for_key(json, "artist");
		char *genre  = json_get_string_value_for_key(json, "genre");

		if (artist) {
			if (pos == 0) listwidget_clear_all_rows(ui->lw_mlib_artists);
			row = listwidget_add_row(ui->lw_mlib_artists) - 1;
			listwidget_set_cell_data(ui->lw_mlib_artists, row, 0, artist);
			ui_mlib_set_state(ui, MLIB_STATE_BROWSE_ARTISTS);
		} else if (genre) {
			if (pos == 0) listwidget_clear_all_rows(ui->lw_mlib_genres);
			row = listwidget_add_row(ui->lw_mlib_genres) - 1;
			listwidget_set_cell_data(ui->lw_mlib_genres, row, 0, genre);
			ui_mlib_set_state(ui, MLIB_STATE_BROWSE_GENRES);
		}
		ui_refresh_active_window(ui);
	}
}

static void cmd_playmode_info(UI *ui, JSON_Object *json)
{
	ui_update_playmode(ui, (int)json_get_number_value_for_key(json, "mode"));
	ui_draw_header(ui);
}

static void cmd_volume_info(UI *ui, JSON_Object *json)
{
	ui_update_volume(ui, (int)json_get_number_value_for_key(json, "volume"));
	ui_draw_header(ui);
}

static int cmd_login(UI *ui, JSON_Object *json, int sock, const char *cur_dir)
{
	int   screen_update = 0;
	char *res = json_get_string_value_for_key(json, "res");
	if (res && strcmp(res, "success") == 0) {
		websocket_send_str(sock, "{\"cmd\":\"trackinfo\"}", 1);
		websocket_send_str(sock, "{\"cmd\":\"playlist_playmode_get_info\"}", 1);
		if (ui) {
			listwidget_clear_all_rows(ui->lw_fb);
			ui_update_playlist_pos(ui, 0);
			if (!cur_dir) {
				websocket_send_str(sock, "{\"cmd\":\"dir_read\", \"dir\": \"/\"}", 1);
			} else {
				char tmp[256];
				snprintf(tmp, 255, "{\"cmd\":\"dir_read\", \"dir\": \"%s\"}", cur_dir);
				websocket_send_str(sock, tmp, 1);
			}
		}
	} else {
		if (ui) {
			wprintw(ui->win_cmd->win, "Login failed! Check password\n");
			ui->active_win = WIN_CMD;
			screen_update = 1;
		}
	}
	return screen_update;
}

static void cmd_playback_state(UI *ui, JSON_Object *json)
{
	int state = json_get_number_value_for_key(json, "state");
	char *str;
	switch (state) {
		default:
		case 0: str = "  "; break; /* stopped */
		case 1: str = "> "; break; /* play */
		case 2: str = "||"; break; /* paused */
	}
	ui_update_playback_status(ui, str);
	ui_draw_header(ui);
}

static void cmd_busy(UI *ui, int busy)
{
	ui_busy_indicator(ui, busy);
	ui_draw_header(ui);
}

static int handle_data_in_ringbuffer(RingBuffer *rb, UI *ui, int sock, char *password, char **cur_dir, char *input)
{
	char tmp_buf[16];
	int  size, loop = 1;
	int  network_error = 0;
	do {
		size = 0;
		/* 0. set unread pos on ringbuffer */
		ringbuffer_set_unread_pos(rb);
		/* 1. read a few bytes from the ringbuffer (Websocket flags+packet size) */
		if (ringbuffer_read(rb, tmp_buf, 10)) {
			/* 2. Check wether the required packet size is available in the ringbuffer */
			size = websocket_calculate_packet_size(tmp_buf);
			ringbuffer_unread(rb);
		}
		/* 3. If so, read the whole packet from the ringbuffer and process it, 
		 *    then startover at step 0. If not, unread the already read bytes 
		 *    from the ringbuffer and leave inner loop so the ringbuffer can fill more */
		if (ringbuffer_get_fill(rb) >= size && size > 0) {
			char *wspacket = malloc(size+1); /* packet size+1 byte null terminator */
			const char *payload;
			if (wspacket) {
				JSON_Object *json;
				ringbuffer_read(rb, wspacket, size);
				wspacket[size] = '\0';
				payload = websocket_get_payload(wspacket);
				json = json_parse_alloc(payload);
				if (!json_object_has_parse_error(json)) {
					char *cmd = json_get_string_value_for_key(json, "cmd");
					if (cmd) {
						int screen_update = 0;
						if (strcmp(cmd, "trackinfo") == 0) {
							cmd_trackinfo(ui, json);
						} else if (strcmp(cmd, "track_change") == 0) {
							cmd_track_change(ui, json);
						} else if (strcmp(cmd, "playback_time") == 0) {
							cmd_playback_time_change(ui, json);
						} else if (strcmp(cmd, "playback_state") == 0) {
							cmd_playback_state(ui, json);
						} else if (strcmp(cmd, "hello") == 0) {
							char tmp[256];
							snprintf(tmp, 255, "{\"cmd\":\"login\",\"password\":\"%s\"}", password);
							websocket_send_str(sock, tmp, 1);
						} else if (strcmp(cmd, "login") == 0) {
							screen_update = cmd_login(ui, json, sock, *cur_dir);
						} else if (strcmp(cmd, "playlist_info") == 0) {
							wprintw(ui->win_cmd->win, "Playlist info received!\n");
						} else if (strcmp(cmd, "playlist_change") == 0) {
							cmd_playlist_change(ui, json, sock);
							screen_update = 1;
						} else if (strcmp(cmd, "playlist_item") == 0) {
							cmd_playlist_item(ui, json, sock);
						} else if (strcmp(cmd, "dir_read") == 0) {
							char *tmp_dir = cmd_dir_read(ui, json);
							if (tmp_dir) {
								if (*cur_dir) free(*cur_dir);
								*cur_dir = tmp_dir;
							}
						} else if (strcmp(cmd, "playmode_info") == 0) {
							cmd_playmode_info(ui, json);
						} else if (strcmp(cmd, "volume_info") == 0) {
							cmd_volume_info(ui, json);
						} else if (strcmp(cmd, "mlib_result") == 0) {
							cmd_mlib_result(ui, json);
						} else if (strcmp(cmd, "mlib_search_start") == 0) {
							cmd_busy(ui, 1);
						} else if (strcmp(cmd, "mlib_search_done") == 0) {
							cmd_busy(ui, 0);
						} else if (strcmp(cmd, "mlib_browse_result") == 0) {
							cmd_mlib_browse_result(ui, json);
						}
						if (screen_update) ui_refresh_active_window(ui);
						ui_cursor_text_input(ui, input);
					}
				} else {
					wprintw(ui->win_cmd->win, "ERROR: Invalid JSON data received.\n");
					network_error = 1;
				}
				json_object_free(json);
				free(wspacket);
			} else {
				loop = 0;
			}
		} else if (size > 0) {
			wprintw(ui->win_cmd->win,
					"Not enough data available. Need %d bytes, but only %d avail.\n",
					size, ringbuffer_get_fill(rb));
			ringbuffer_unread(rb);
			loop = 0;
		} else {
			loop = 0;
			if (size == -1) network_error = 1;
		}
	} while (loop && !network_error);
	return network_error;
}

static volatile sig_atomic_t quit = 0;

static void sig_handler(int sig)
{
	quit = 1;
}

static void gmuc_help(const char *str)
{
	printf("Usage: %s [-c /path/to/gmuc.conf] [-h] [-p [format string]] [-u] [command [arguments]]\n", str);
	printf(
		"When started without a command and without the -p flag the interactive gmuc\n" \
		"user interface will be launched.\n" \
		"The -p flag causes gmuc to print the current track and exit or if the -u flag\n" \
		"is also specified continously print the current track.\n"
	);
	exit(0);
}

static Function get_function_from_button(UI *ui, int res, wint_t ch)
{
	int      i;
	Function func = FUNC_NONE;
	for (i = 0; ui->fb_visible && ui->fb_visible[i].button_name; i++) {
		if (ui->fb_visible[i].key == ch) {
			if ((res == KEY_CODE_YES && ui->fb_visible[i].keycode) ||
				(res != KEY_CODE_YES && !ui->fb_visible[i].keycode))
				func = ui->fb_visible[i].func;
			break;
		}
	}
	return func;
}

static void playlist_handle_return_key(UI *ui, int sock)
{
	char str[128];
	snprintf(str, 127, "{\"cmd\":\"play\", \"item\":%d}", listwidget_get_selection(ui->lw_pl));
	websocket_send_str(sock, str, 1);
}

static void file_browser_handle_return_key(UI *ui, int sock, const char *cur_dir)
{
	char  tmp[256];
	int   sel_row = listwidget_get_selection(ui->lw_fb);
	char *ftype = NULL;

	if (sel_row >= 0)
		ftype = listwidget_get_row_data(ui->lw_fb, sel_row, 0);

	if (ftype && strcmp(ftype, "[DIR]") == 0) {
		char *dir = dir_get_new_dir_alloc(
			cur_dir ? cur_dir : "/", 
			listwidget_get_row_data(ui->lw_fb, sel_row, 1)
		);
		wprintw(ui->win_cmd->win, "Selected dir: %s/%d\n", listwidget_get_row_data(ui->lw_fb, sel_row, 1), sel_row);
		wprintw(ui->win_cmd->win, "Full path: %s\n", cur_dir);
		if (dir) {
			snprintf(tmp, 255, "{\"cmd\":\"dir_read\", \"dir\": \"%s\"}", dir);
			websocket_send_str(sock, tmp, 1);
			free(dir);
		}
	} else { /* Add file */
		/* TODO */
	}
}

static void media_library_play_item(UI *ui, int sock)
{
	if (ui_mlib_get_state(ui) == MLIB_STATE_RESULTS) {
		char  tmp[256] = "";
		int   s = listwidget_get_selection(ui->lw_mlib_search);
		char *idstr = listwidget_get_row_data(ui->lw_mlib_search, s, 3);
		int   id = idstr ? atoi(idstr) : -1;
		wprintw(ui->win_cmd->win, "Playing medialib ID %d...\n", id);
		snprintf(tmp, 255, "{\"cmd\":\"play\", \"source\":\"medialib\", \"item\": %d}", id);
		if (tmp[0]) websocket_send_str(sock, tmp, 1);
	}
}

static void media_library_handle_return_key(UI *ui, int sock)
{
	char  tmp[256] = "";
	switch (ui_mlib_get_state(ui)) {
		case MLIB_STATE_RESULTS: {
			int   s = listwidget_get_selection(ui->lw_mlib_search);
			char *idstr = listwidget_get_row_data(ui->lw_mlib_search, s, 3);
			int   id = idstr ? atoi(idstr) : -1;
			wprintw(ui->win_cmd->win, "Add medialib ID %d to playlist...\n", id);
			snprintf(tmp, 255, "{\"cmd\":\"medialib_add_id_to_playlist\", \"id\": %d}", id);
			break;
		}
		case MLIB_STATE_BROWSE_ARTISTS: {
			int   s = listwidget_get_selection(ui->lw_mlib_artists);
			char *str = listwidget_get_row_data(ui->lw_mlib_artists, s, 0);
			char *str_esc = json_string_escape_alloc(str);
			if (snprintf(tmp, 255, "{\"cmd\":\"medialib_search\", \"type\": \"0\", \"str\": \"%s\"}", str_esc) < 255) {
				ui_mlib_set_state(ui, MLIB_STATE_RESULTS);
				ui_refresh_active_window(ui);
			} else {
				tmp[0] = '\0';
			}
			break;
		}
		case MLIB_STATE_BROWSE_GENRES:
			break;
	}
	if (tmp[0]) websocket_send_str(sock, tmp, 1);
}

static void initiate_websocket_handshake(int sock, char *host)
{
	char        str2[1024];
	char       *key;
	const char *str = "GET /gmu HTTP/1.1\r\n"
	                  "Host: %s\r\n"
	                  "Upgrade: websocket\r\n"
	                  "Connection: Upgrade\r\n"
	                  "Sec-WebSocket-Key: %s\r\n"
	                  "Sec-WebSocket-Version: 13\r\n\r\n";
	key = websocket_client_generate_sec_websocket_key_alloc();
	if (key) {
		snprintf(str2, 1023, str, host, key);
		send(sock, str2, strlen(str2), 0);
		free(key);
	}
}

/**
 * Used to set the text of the command/search text input element
 */
static int set_input_text(
	const char  *input_text,
	wchar_t     *wchars,
	unsigned int wchars_max_length,
	char       **input
)
{
	unsigned int len = strlen(input_text);
	if (len < wchars_max_length) {
		mbstowcs(wchars, "search ", len);
		wchars[len] = L'\0';
		*input = wchars_to_utf8_str_realloc(*input, wchars);
	} else {
		len = 0;
	}
	return len;
}

/**
 * Returns pointer to string if input has been changed, or NULL otherwise
 */
static char *handle_function_based_on_key_press(
	wint_t      ch,      /* Pressed key */
	UI         *ui,
	int         sock,
	const char *cur_dir,
	int         res,
	State       state,
	wchar_t    *wchars,
	int        *cpos     /* Cursor position in input field */
)
{
	char *input = NULL;
	Function func = get_function_from_button(ui, res, ch);
	switch (func) {
		case FUNC_NEXT_WINDOW:
			ui_active_win_next(ui);
			break;
		case FUNC_FB_ADD: {
			char  cmd[256];
			int   sel_row = listwidget_get_selection(ui->lw_fb);
			char *ftype = listwidget_get_row_data(ui->lw_fb, sel_row, 0);
			char *file = listwidget_get_row_data(ui->lw_fb, sel_row, 1);
			char *type = strcmp(ftype, "[DIR]") == 0 ? "dir" : "file";
			char *cur_dir_esc = json_string_escape_alloc(cur_dir);
			char *file_esc = json_string_escape_alloc(file);
			if (cur_dir_esc && file_esc) {
				snprintf(
					cmd,
					255, 
					"{\"cmd\":\"playlist_add\",\"path\":\"%s/%s\",\"type\":\"%s\"}",
					cur_dir_esc,
					file_esc,
					type
				);
				websocket_send_str(sock, cmd, 1);
			}
			free(cur_dir_esc);
			free(file_esc);
			break;
		}
		case FUNC_FB_MLIB_ADD_PATH: {
			char  cmd[256];
			int   sel_row = listwidget_get_selection(ui->lw_fb);
			const char *ftype = listwidget_get_row_data(ui->lw_fb, sel_row, 0);
			const char *file = listwidget_get_row_data(ui->lw_fb, sel_row, 1);
			if (strcmp(ftype, "[DIR]") == 0) {
				char *cur_dir_esc = json_string_escape_alloc(cur_dir);
				char *file_esc = json_string_escape_alloc(file);
				if (cur_dir_esc && file_esc) {
					snprintf(
						cmd,
						255, 
						"{\"cmd\":\"medialib_path_add\",\"path\":\"%s/%s\"}",
						cur_dir_esc,
						file_esc
					);
					websocket_send_str(sock, cmd, 1);
				}
				free(cur_dir_esc);
				free(file_esc);
			}
			break;
		}
		case FUNC_PREVIOUS:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"prev\"}", 1);
			}
			break;
		case FUNC_NEXT:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"next\"}", 1);
			}
			break;
		case FUNC_STOP:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"stop\"}", 1);
			}
			break;
		case FUNC_PAUSE:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"pause\"}", 1);
			}
			break;
		case FUNC_PLAY:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"play\"}", 1);
			}
			break;
		case FUNC_PLAY_PAUSE:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"play_pause\"}", 1);
			}
			break;
		case FUNC_PLAYMODE:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"playlist_playmode_cycle\"}", 1);
			}
			break;
		case FUNC_PL_CLEAR:
			if (state == STATE_CONNECTION_ESTABLISHED) {
				websocket_send_str(sock, "{\"cmd\":\"playlist_clear\"}", 1);
			}
			break;
		case FUNC_PL_DEL_ITEM: {
			char str[128];
			snprintf(str, 127, "{\"cmd\":\"playlist_item_delete\",\"item\":%d}", 
					 listwidget_get_selection(ui->lw_pl));
			websocket_send_str(sock, str, 1);
			break;
		}
		case FUNC_TEXT_INPUT:
			ui_enable_text_input(ui, 1);
			break;
		case FUNC_SEARCH:
			/* Set text to "search " */
			*cpos = set_input_text("search ", wchars, 256, &input);
			ui_mlib_set_state(ui, MLIB_STATE_RESULTS);
			ui_enable_text_input(ui, 1);
			break;
		case FUNC_MLIB_REFRESH:
			websocket_send_str(sock, "{\"cmd\":\"medialib_refresh\"}", 1);
			break;
		case FUNC_MLIB_PLAY_ITEM:
			media_library_play_item(ui, sock);
			break;
		case FUNC_VOLUME_UP:
			websocket_send_str(sock, "{\"cmd\":\"volume_set\",\"relative\":1}", 1);
			break;
		case FUNC_VOLUME_DOWN:
			websocket_send_str(sock, "{\"cmd\":\"volume_set\",\"relative\":-1}", 1);
			break;
		case FUNC_TOGGLE_TIME:
			ui_toggle_time_display(ui);
			ui_draw_header(ui);
			window_refresh(ui->win_header);
			break;
		case FUNC_BROWSE_ARTISTS:
			ui_mlib_set_state(ui, MLIB_STATE_BROWSE_ARTISTS);
			websocket_send_str(sock, "{\"cmd\":\"medialib_browse\",\"column\":\"artist\"}", 1);
			break;
		case FUNC_BROWSE_GENRES:
			ui_mlib_set_state(ui, MLIB_STATE_BROWSE_GENRES);
			websocket_send_str(sock, "{\"cmd\":\"medialib_browse\",\"column\":\"genre\"}", 1);
			break;
		case FUNC_QUIT:
			quit = 1;
		default:
			break;
	}
	return input;
}

static void handle_key_press(wint_t ch, UI *ui, int sock, const char *cur_dir)
{
	switch (ch) {
		case KEY_DOWN:
			flushinp();
			switch (ui->active_win) {
				case WIN_PL:
					listwidget_move_cursor(ui->lw_pl, 1);
					break;
				case WIN_FB:
					listwidget_move_cursor(ui->lw_fb, 1);
					break;
				case WIN_LIB:
					switch (ui_mlib_get_state(ui)) {
						case MLIB_STATE_RESULTS:
							listwidget_move_cursor(ui->lw_mlib_search, 1);
							break;
						case MLIB_STATE_BROWSE_ARTISTS:
							listwidget_move_cursor(ui->lw_mlib_artists, 1);
							break;
						case MLIB_STATE_BROWSE_GENRES:
							listwidget_move_cursor(ui->lw_mlib_genres, 1);
							break;
					}
					break;
				default:
					break;
			}
			break;
		case KEY_UP:
			flushinp();
			switch (ui->active_win) {
				case WIN_PL:
					listwidget_move_cursor(ui->lw_pl, -1);
					break;
				case WIN_FB:
					listwidget_move_cursor(ui->lw_fb, -1);
					break;
				case WIN_LIB:
					switch (ui_mlib_get_state(ui)) {
						case MLIB_STATE_RESULTS:
							listwidget_move_cursor(ui->lw_mlib_search, -1);
							break;
						case MLIB_STATE_BROWSE_ARTISTS:
							listwidget_move_cursor(ui->lw_mlib_artists, -1);
							break;
						case MLIB_STATE_BROWSE_GENRES:
							listwidget_move_cursor(ui->lw_mlib_genres, -1);
							break;
					}
					break;
				default:
					break;
			}
			break;
		case KEY_NPAGE:
			flushinp();
			switch (ui->active_win) {
				case WIN_PL:
					listwidget_move_cursor(ui->lw_pl, ui->lw_pl->win->height-2);
					break;
				case WIN_FB:
					listwidget_move_cursor(ui->lw_fb, ui->lw_pl->win->height-2);
					break;
				case WIN_LIB:
					switch (ui_mlib_get_state(ui)) {
						case MLIB_STATE_RESULTS:
							listwidget_move_cursor(ui->lw_mlib_search, ui->lw_mlib_search->win->height-2);
							break;
						case MLIB_STATE_BROWSE_ARTISTS:
							listwidget_move_cursor(ui->lw_mlib_artists, ui->lw_mlib_search->win->height-2);
							break;
						case MLIB_STATE_BROWSE_GENRES:
							listwidget_move_cursor(ui->lw_mlib_genres, ui->lw_mlib_search->win->height-2);
							break;
					}
					break;
				default:
					break;
			}
			break;
		case KEY_PPAGE:
			flushinp();
			switch (ui->active_win) {
				case WIN_PL:
					listwidget_move_cursor(ui->lw_pl, -(ui->lw_pl->win->height-2));
					break;
				case WIN_FB:
					listwidget_move_cursor(ui->lw_fb, -(ui->lw_pl->win->height-2));
					break;
				case WIN_LIB:
					switch (ui_mlib_get_state(ui)) {
						case MLIB_STATE_RESULTS:
							listwidget_move_cursor(ui->lw_mlib_search, -(ui->lw_mlib_search->win->height-2));
							break;
						case MLIB_STATE_BROWSE_ARTISTS:
							listwidget_move_cursor(ui->lw_mlib_artists, -(ui->lw_mlib_search->win->height-2));
							break;
						case MLIB_STATE_BROWSE_GENRES:
							listwidget_move_cursor(ui->lw_mlib_genres, -(ui->lw_mlib_search->win->height-2));
							break;
					}
					break;
				default:
					break;
			}
			break;
		case KEY_HOME:
			switch (ui->active_win) {
				case WIN_PL:
					listwidget_set_cursor(ui->lw_pl, 0);
					break;
				case WIN_FB:
					listwidget_set_cursor(ui->lw_fb, 0);
					break;
				case WIN_LIB:
					switch (ui_mlib_get_state(ui)) {
						case MLIB_STATE_RESULTS:
							listwidget_set_cursor(ui->lw_mlib_search, 0);
							break;
						case MLIB_STATE_BROWSE_ARTISTS:
							listwidget_set_cursor(ui->lw_mlib_artists, 0);
							break;
						case MLIB_STATE_BROWSE_GENRES:
							listwidget_set_cursor(ui->lw_mlib_genres, 0);
							break;
					}
					break;
				default:
					break;
			}
			break;
		case KEY_END:
			switch (ui->active_win) {
				case WIN_PL:
					listwidget_set_cursor(ui->lw_pl, LW_CURSOR_POS_END);
					break;
				case WIN_FB:
					listwidget_set_cursor(ui->lw_fb, LW_CURSOR_POS_END);
					break;
				case WIN_LIB:
					switch (ui_mlib_get_state(ui)) {
						case MLIB_STATE_RESULTS:
							listwidget_set_cursor(ui->lw_mlib_search, LW_CURSOR_POS_END);
							break;
						case MLIB_STATE_BROWSE_ARTISTS:
							listwidget_set_cursor(ui->lw_mlib_artists, LW_CURSOR_POS_END);
							break;
						case MLIB_STATE_BROWSE_GENRES:
							listwidget_set_cursor(ui->lw_mlib_genres, LW_CURSOR_POS_END);
							break;
					}
					break;
				default:
					break;
			}
			break;
		case '\n': {
			switch (ui->active_win) {
				case WIN_PL:
					playlist_handle_return_key(ui, sock);
					break;
				case WIN_FB:
					file_browser_handle_return_key(ui, sock, cur_dir);
					break;
				case WIN_LIB:
					media_library_handle_return_key(ui, sock);
					break;
				default:
					break;
			}
			break;
		}
	}
}

static void execute_command(int sock, Command cmd, const char *params, UI *ui)
{
	char *str = NULL;
	int   free_str = 0;
	switch (cmd) {
		case PLAY:
			if (!params) {
				str = "{\"cmd\":\"play\"}";
			} else {
				char *file_esc = json_string_escape_alloc(params);

				str = "";
				if (file_esc) {
					size_t len = strlen(file_esc);
					if (len > 0) {
						str = malloc(64 + len);
						if (str) {
							free_str = 1;
							snprintf(str, 255, "{\"cmd\":\"play\", \"source\":\"file\", \"file\": \"%s\"}", file_esc);
						}
					}
					free(file_esc);
				}
			}
			break;
		case PAUSE:
			str = "{\"cmd\":\"pause\"}";
			break;
		case NEXT:
			str = "{\"cmd\":\"next\"}";
			break;
		case PREVIOUS:
			str = "{\"cmd\":\"prev\"}";
			break;
		case STOP:
			str = "{\"cmd\":\"stop\"}";
			break;
		case FILES:
			str = "{\"cmd\":\"dir_read\", \"dir\": \"/\"}";
			break;
		case ADD:
			str = malloc(320);
			if (str) {
				char *params_esc = json_string_escape_alloc(params);
				if (params_esc) {
					if (snprintf(str, 320, "{\"cmd\":\"playlist_add\",\"path\":\"%s\",\"type\":\"file\"}", params_esc) == 320) {
						free(str);
						str = NULL;
					}
					free(params_esc);
				} else {
					free(str);
					str = NULL;
				}
				free_str = 1;
			}
			break;
		case SEARCH:
			if (ui) {
				str = malloc(320);
				if (str) {
					char *params_esc = json_string_escape_alloc(params);
					if (params_esc) {
						if (snprintf(str, 320, "{\"cmd\":\"medialib_search\",\"type\":\"0\",\"str\":\"%s\"}", params_esc) == 320) {
							free(str);
							str = NULL;
						}
						free(params_esc);
						listwidget_clear_all_rows(ui->lw_mlib_search);
					} else {
						free(str);
						str = NULL;
					}
					free_str = 1;
				}
				ui_active_win_set(ui, WIN_LIB);
			}
			break;
		case PING:
			str = "{\"cmd\":\"ping\"}";
			break;
		default:
			break;
	}
	if (str) websocket_send_str(sock, str, 1);
	if (free_str) free(str);
}

static int run_gmuc_ui(int color, char *host, char *password)
{
	int     res = EXIT_FAILURE;
	int     network_error = 0;
	UI      ui;
	char   *cur_dir = NULL;
	int     sock;
	ssize_t size;
	char   *buffer = malloc(BUF);

	assign_signal_handler(SIGWINCH, sig_handler_sigwinch);

	ui_init(&ui, color);
	ui_draw(&ui);
	ui_cursor_text_input(&ui, NULL);
	ui_enable_text_input(&ui, 0);

	while (!quit) {
		ui_update_trackinfo(&ui, host, "Trying to connect to Gmu server", NULL, NULL);
		ui_draw_header(&ui);
		if (!buffer) {
			quit = 1;
		} else if ((sock = nethelper_tcp_connect_to_host(host, PORT, 0)) > 0) {
			struct timeval tv;
			fd_set         readfds, errorfds;
			char          *input = NULL;
			int            cpos = 0;
			RingBuffer     rb;
			int            r = 1, connected = 1;
			State          state = STATE_WEBSOCKET_HANDSHAKE;
			wchar_t        wchars[256];
			size_t         ping_timeout = PING_TIMEOUT, pong_timeout = PONG_TIMEOUT;

			network_error = 0;
			wchars[0] = L'\0';

			initiate_websocket_handshake(sock, host);

			ringbuffer_init(&rb, 65536);
			while (!quit && connected && !network_error && pong_timeout) {
				FD_ZERO(&readfds);
				FD_ZERO(&errorfds);
				FD_SET(fileno(stdin), &readfds);
				FD_SET(sock, &readfds);
				tv.tv_sec = 0;
				tv.tv_usec = 500000;
				if (select(sock+1, &readfds, NULL, &errorfds, &tv) < 0) {
					/* ERROR in select() */
					FD_ZERO(&readfds);
				}

				if (resized) {
					resized = 0;
					ui_resize(&ui);
				}

				if (FD_ISSET(fileno(stdin), &readfds)) {
					char   buf[1024];
					wint_t ch;
					int    res;

					memset(buf, 0, 1024);
					res = wget_wch(stdscr, &ch);
					if (res == OK && ui.text_input_enabled) {
						ui_refresh_active_window(&ui);
						switch (ch) {
							case '\n': {
								int len = wcstombs(NULL, wchars, 0);
								ui_enable_text_input(&ui, 0);
								ui_draw_footer(&ui);
								if (len > 0) {
									input = wchars_to_utf8_str_realloc(input, wchars);
									if (input) {
										Command cmd = NO_COMMAND;
										char   *params = NULL;

										if (len > 0) {
											wprintw(ui.win_cmd->win, "%s\n", input);
											cpos = 0;
											parse_input_alloc(input, &cmd, &params);
											if (state == STATE_CONNECTION_ESTABLISHED) {
												execute_command(sock, cmd, params, &ui);
											} else {
												wdprintf(V_INFO, "gmuc", "Connection not established. Cannot send command.\n");
											}
										}
										wprintw(ui.win_cmd->win, "Command translates to %d", cmd);
										if (params) {
											wprintw(ui.win_cmd->win, " with parameters '%s'", params);
											free(params);
										}
										wprintw(ui.win_cmd->win, ".\n", cmd);
										free(input);
										input = NULL;
										ui_cursor_text_input(&ui, NULL);
									} else {
										wprintw(ui.win_cmd->win, "Error when processing input text. :(\n");
									}
									wchars[0] = L'\0';
								}
								ui_refresh_active_window(&ui);
								ui_cursor_text_input(&ui, NULL);
								window_refresh(ui.win_footer);
								break;
							}
							case KEY_BACKSPACE:
							case '\b':
							case KEY_DC:
							case 127: {
								if (cpos > 0) {
									wchars[cpos-1] = L'\0';
									cpos--;
								}
								input = wchars_to_utf8_str_realloc(input, wchars);
								if (input) {
									ui_refresh_active_window(&ui);
									ui_cursor_text_input(&ui, input);
								} else {
									wprintw(ui.win_cmd->win, "Error when processing input text. :(\n");
								}
								window_refresh(ui.win_footer);
								break;
							}
							case KEY_RESIZE:
								flushinp();
								break;
							default: {
								wchars[cpos] = ch;
								wchars[cpos+1] = L'\0';
								cpos++;
								input = wchars_to_utf8_str_realloc(input, wchars);
								ui_cursor_text_input(&ui, input);
								window_refresh(ui.win_footer);
								break;
							}
						}
					} else if (res == OK || res == KEY_CODE_YES) {
						char *in = handle_function_based_on_key_press(ch, &ui, sock, cur_dir, res, state, wchars, &cpos);
						if (in) input = in;
						handle_key_press(ch, &ui, sock, cur_dir);
						ui_refresh_active_window(&ui);
						ui_cursor_text_input(&ui, input);
						window_refresh(ui.win_footer);
					} else {
						wprintw(ui.win_cmd->win, "ERROR\n");
					}
				}
				if (FD_ISSET(sock, &readfds) && !FD_ISSET(sock, &errorfds)) {
					ping_timeout = PING_TIMEOUT;
					pong_timeout = PONG_TIMEOUT;
					if (r) {
						memset(buffer, 0, BUF); /* we don't need that later */
						size = recv(sock, buffer, BUF-1, 0);
						if (size <= 0) {
							wprintw(ui.win_cmd->win, "Network Error: Data size = %d Error: %s\n", size, strerror(errno));
							if (size == -1) wprintw(ui.win_cmd->win, "Error: %s\n", strerror(errno));
							ui_update_trackinfo(&ui, "Gmu Network Error", strerror(errno), NULL, NULL);
							ui_draw_header(&ui);
							network_error = 1;
						}
						if (size > 0) r = ringbuffer_write(&rb, buffer, size);
						if (!r) wprintw(ui.win_cmd->win, "Write failed, ringbuffer full!\n");
					} else {
						wprintw(ui.win_cmd->win, "Can't read more from socket, ringbuffer full!\n");
					}
				} else { /* No data received */
					if (ping_timeout > 0)
						ping_timeout--;
					if (ping_timeout == 0) {
						if (pong_timeout == PONG_TIMEOUT) { /* Send PING request */
							execute_command(sock, PING, NULL, NULL);
						}
						pong_timeout--; /* If this reaches 0, exit loop and try to reconnect */
					}
				}

				switch (state) {
					case STATE_WEBSOCKET_HANDSHAKE: {
						state = do_websocket_handshake(&rb, state);
						break;
					}
					case STATE_CONNECTION_ESTABLISHED: {
						if (!network_error)
							network_error = handle_data_in_ringbuffer(&rb, &ui, sock, password, &cur_dir, input);
						break;
					}
					case STATE_WEBSOCKET_HANDSHAKE_FAILED:
						network_error = 1;
						connected = 0;
						break;
				}
			}
			if (input) free(input);
			ringbuffer_free(&rb);
			close(sock);
			res = EXIT_SUCCESS;
		}
		usleep(500000);
	}
	ui_free(&ui);
	free(cur_dir);
	if (buffer) free(buffer);
	return res;
}

/**
 * Prints track information for the current track, formatted according to
 * the supplied format string.
 */
static void cmd_trackinfo_stdout(JSON_Object *json, const char *format_str)
{
	char  *artist  = json_get_string_value_for_key(json, "artist");
	char  *title   = json_get_string_value_for_key(json, "title");
	char  *album   = json_get_string_value_for_key(json, "album");
	char  *date    = json_get_string_value_for_key(json, "date");
	int    minutes = (int)json_get_number_value_for_key(json, "length_min");
	int    seconds = (int)json_get_number_value_for_key(json, "length_sec");
	size_t i, len = strlen(format_str);
	int    empty = 0;

	for (i = 0; i < len; i++) {
		if (format_str[i] != '%') {
			putc(format_str[i], stdout);
		} else {
			i++;
			if (i < len) {
				switch (format_str[i]) {
					case 'a': /* artist */
						empty = 0;
						if (strlen(artist) > 0)
							fputs(artist, stdout);
						else
							empty = 1;
						break;
					case 't': /* title */
						empty = 0;
						if (strlen(title) > 0)
							fputs(title, stdout);
						else
							empty = 1;
						break;
					case 'A': /* album */
						empty = 0;
						if (strlen(album) > 0)
							fputs(album, stdout);
						else
							empty = 1;
						break;
					case 'm': /* minutes */
						empty = 0;
						printf("%02d", minutes);
						break;
					case 's': /* seconds */
						empty = 0;
						printf("%02d", seconds);
						break;
					case 'd': /* date */
						empty = 0;
						if (strlen(date) > 0)
							fputs(date, stdout);
						else
							empty = 1;
						break;
					case 'S': /* separator ' - ' unless previous format replacement was empty */
						if (!empty) {
							fputs(" - ", stdout);
						}
						break;
					case '%':
						fputs("%", stdout);
						break;
					default: /* unknown format character */
						break;
				}
			}
		}
	}
	putc('\n', stdout);
}

/**
 * Returns 1 when printing is done or if an error condition occured or
 * 0 if there is still work to do.
 */
static int handle_data_in_ringbuffer_print_only(
	RingBuffer *rb,
	int         sock,
	const char *password,
	char      **cur_dir,
	const char *format_str,
	int         silent
)
{
	char tmp_buf[16];
	int  size, loop = 1;
	int  res = 0;
	do {
		size = 0;
		/* 0. set unread pos on ringbuffer */
		ringbuffer_set_unread_pos(rb);
		/* 1. read a few bytes from the ringbuffer (Websocket flags+packet size) */
		if (ringbuffer_read(rb, tmp_buf, 10)) {
			/* 2. Check wether the required packet size is available in the ringbuffer */
			size = websocket_calculate_packet_size(tmp_buf);
			ringbuffer_unread(rb);
		}
		/* 3. If so, read the whole packet from the ringbuffer and process it,
		 *    then startover at step 0. If not, unread the already read bytes
		 *    from the ringbuffer and leave inner loop so the ringbuffer can fill more */
		if (ringbuffer_get_fill(rb) >= size && size > 0) {
			char *wspacket = malloc(size+1); /* packet size+1 byte null terminator */
			const char *payload;
			if (wspacket) {
				JSON_Object *json;
				ringbuffer_read(rb, wspacket, size);
				wspacket[size] = '\0';
				payload = websocket_get_payload(wspacket);
				json = json_parse_alloc(payload);
				if (!json_object_has_parse_error(json)) {
					char *cmd = json_get_string_value_for_key(json, "cmd");
					if (cmd) {
						if (strcmp(cmd, "trackinfo") == 0) {
							if (!silent)
								cmd_trackinfo_stdout(json, format_str ? format_str : "%a%S%t (%A) [%m:%s]");
							res = 1;
						} else if (strcmp(cmd, "track_change") == 0) {
							/* TODO */
						} else if (strcmp(cmd, "playback_time") == 0) {
							/* TODO */
						} else if (strcmp(cmd, "playback_state") == 0) {
							/* TODO */
						} else if (strcmp(cmd, "hello") == 0) {
							char tmp[256];
							snprintf(tmp, 255, "{\"cmd\":\"login\",\"password\":\"%s\"}", password);
							websocket_send_str(sock, tmp, 1);
						} else if (strcmp(cmd, "login") == 0) {
							cmd_login(NULL, json, sock, *cur_dir);
						} else if (strcmp(cmd, "playmode_info") == 0) {
							/* TODO */
						} else if (strcmp(cmd, "volume_info") == 0) {
							/* TODO */
						}
					}
				} else {
					res = 1; /* error (invalid JSON data received) */
				}
				json_object_free(json);
				free(wspacket);
			} else {
				loop = 0;
			}
		} else if (size > 0) {
			ringbuffer_unread(rb);
			loop = 0;
		} else {
			loop = 0;
			if (size == -1) res = 1;
		}
	} while (loop && !res);
	return res;
}

static int process_cli_command(
	char       *host,
	char       *password,
	int         just_once,
	Command     command,
	const char *params,
	const char *format_str
)
{
	int     res = EXIT_FAILURE;
	int     network_error = 0;
	char   *cur_dir = NULL;
	int     sock;
	ssize_t size;
	char   *buffer = malloc(BUF);

	while (!quit) {
		if (!buffer) {
			quit = 1;
		} else if ((sock = nethelper_tcp_connect_to_host(host, PORT, 0)) > 0) {
			struct timeval tv;
			fd_set         readfds, errorfds;
			RingBuffer     rb;
			int            r = 1, connected = 1;
			State          state = STATE_WEBSOCKET_HANDSHAKE;
			size_t         ping_timeout = PING_TIMEOUT, pong_timeout = PONG_TIMEOUT;

			network_error = 0;

			initiate_websocket_handshake(sock, host);

			ringbuffer_init(&rb, 65536);
			while (!quit && connected && !network_error && pong_timeout) {
				FD_ZERO(&readfds);
				FD_ZERO(&errorfds);
				FD_SET(sock, &readfds);
				tv.tv_sec = 0;
				tv.tv_usec = 100000;
				if (select(sock+1, &readfds, NULL, &errorfds, &tv) < 0) {
					/* ERROR in select() */
					FD_ZERO(&readfds);
				}
				if (FD_ISSET(sock, &readfds) && !FD_ISSET(sock, &errorfds)) {
					ping_timeout = PING_TIMEOUT;
					pong_timeout = PONG_TIMEOUT;
					if (r) {
						memset(buffer, 0, BUF); /* we don't need that later */
						size = recv(sock, buffer, BUF-1, 0);
						if (size <= 0) {
							network_error = 1;
						}
						if (size > 0) r = ringbuffer_write(&rb, buffer, size);
					}
				} else { /* No data received */
					if (ping_timeout > 0)
						ping_timeout--;
					if (ping_timeout == 0) {
						if (pong_timeout == PONG_TIMEOUT) { /* Send PING request */
							execute_command(sock, PING, NULL, NULL);
						}
						pong_timeout--; /* If this reaches 0, exit loop and try to reconnect */
					}
				}

				switch (state) {
					case STATE_WEBSOCKET_HANDSHAKE: {
						state = do_websocket_handshake(&rb, state);
						break;
					}
					case STATE_CONNECTION_ESTABLISHED: {
						int tmp;
						if (!network_error) {
							if (command != NO_COMMAND) {
								execute_command(sock, command, params, NULL);
							}
							tmp = handle_data_in_ringbuffer_print_only(
								&rb,
								sock,
								password,
								&cur_dir,
								format_str,
								command != NO_COMMAND
							);
							if (just_once || command != NO_COMMAND) quit = tmp;
						}
						break;
					}
					case STATE_WEBSOCKET_HANDSHAKE_FAILED:
						network_error = 1;
						connected = 0;
						break;
				}
			}
			ringbuffer_free(&rb);
			close(sock);
			res = EXIT_SUCCESS;
		}
		if (just_once || command != NO_COMMAND) {
			quit = 1;
		} else {
			usleep(500000);
		}
	}
	free(cur_dir);
	if (buffer) free(buffer);
	return res;
}

int main(int argc, char **argv)
{
	int         res = EXIT_FAILURE;
	char       *tmp;
	ConfigFile *config;
	char       *password, *host;
	char       *config_file_path = NULL;
	int         mode_info = 0, just_once = 1;
	const char *format_str = NULL;
	Command     command = NO_COMMAND;
	char       *params = NULL;

	assign_signal_handler(SIGINT, sig_handler);
	assign_signal_handler(SIGTERM, sig_handler);
	assign_signal_handler(SIGPIPE, SIG_IGN);

	config = cfg_init();
	cfg_add_key(config, "Host", "127.0.0.1");
	cfg_add_key(config, "Password", "stupidpassword");
	cfg_add_key(config, "Color", "yes");

	if (argc > 1) {
		size_t i;
		for (i = 1; i < argc; i++) {
			if (strlen(argv[i]) >= 2 && argv[i][0] == '-') { /* Parameter (beginning with a '-') */
				switch (argv[i][1]) {
					case 'h':
						gmuc_help(argv[0]);
						break;
					case 'c':
						if (i >= argc-1) {
							gmuc_help(argv[0]);
						} else {
							int len = strlen(argv[i+1]);
							if (len > 255) {
								printf("ERROR: Path too long.\n");
								exit(1);
							} else {
								config_file_path = malloc(len+1);
								if (config_file_path)
									strncpy(config_file_path, argv[i+1], len+1);
							}
							i++;
						}
						break;
					case 'p': /* Print track information on stdout */
						mode_info = 1;
						if (i + 1 < argc && argv[i + 1][0] != '-') {
							format_str = argv[i + 1];
							i++;
						}
						break;
					case 'u': /* Continue running after printing the track information */
						just_once = 0;
						break;
					default:
						gmuc_help(argv[0]);
						break;
				}
			} else { /* Execute command (play/pause/etc.) */
				char  *command_str = NULL;
				size_t command_str_len = 1;

				mode_info = 1;
				command_str = malloc(1);
				if (command_str) command_str[0] = '\0';
				while (i < argc && argv[i][0] != '-') {
					char *tmp, *argument;
					int   argument_is_file = 0;
					char  path[PATH_MAX];
					/* Check if argument is a file */
					if (strchr(argv[i], '.')) { /* Argument contains a dot */
						if (file_exists(argv[i])) argument_is_file = 1;
					}
					if (!argument_is_file) {
						argument = argv[i];
					} else {
						if (realpath(argv[i], path))
							argument = path;
						else
							argument = NULL;
					}
					if (argument) {
						command_str_len += strlen(argument) + 1;
						tmp = realloc(command_str, command_str_len);
						if (tmp) {
							command_str = tmp;
							if (command_str) {
								strcat(command_str, argument);
								if (i + 1 < argc) strcat(command_str, " ");
							}
						} else {
							break;
						}
					}
					i++;
				}
				if (command_str) {
					if (!parse_input_alloc(command_str, &command, &params))
						command = NO_COMMAND;
					if (command_str) free(command_str);
				}
			}
		}
	}

	if (!config_file_path)
		config_file_path = get_config_dir_with_name_alloc("gmu", 1, "gmuc.conf");
	if (cfg_read_config_file(config, config_file_path) != 0) {
		wdprintf(
			V_INFO,
			"gmuc",
			"No config file found. Creating a config file at %s. Please edit that file and try again.\n",
			config_file_path
		);
		if (cfg_write_config_file(config, config_file_path))
			wdprintf(V_ERROR, "gmuc", "ERROR: Unable to create config file.\n");
		cfg_free(config);
		exit(2);
	}
	host = cfg_get_key_value(config, "Host");
	password = cfg_get_key_value(config, "Password");
	if (!host || !password) {
		wdprintf(V_ERROR, "gmuc", "ERROR: Invalid configuration.\n");
		exit(4);
	}
	if (config_file_path) free(config_file_path);

	wdprintf_set_verbosity(V_SILENT);
	if (!mode_info) {
		set_terminal_title("Gmu");
		tmp = cfg_get_key_value(config, "Color");
		if (argc >= 1) res = run_gmuc_ui(tmp && strcmp(tmp, "yes") == 0, host, password);
	} else {
		res = process_cli_command(host, password, just_once, command, params, format_str);
	}
	if (params) free(params);
	cfg_free(config);
	return res;
}

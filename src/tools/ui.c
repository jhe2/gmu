/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
 *
 * File: ui.c  Created: 121209
 *
 * Description: Gmu command line tool
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#include <locale.h>
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <curses.h>
#include <string.h>
#include "ui.h"
#include "window.h"
#include "listwidget.h"
#include "../playlist.h"

static FooterButtons fb_pl[] = {
	{ "Tab", "Window",   FUNC_NEXT_WINDOW, '\t', 0 },
	{ "b",   "Prev",     FUNC_PREVIOUS,    'b', 0 },
	{ "S",   "Stop",     FUNC_STOP,        'S', 0 },
	{ "p",   "Pl/Pause", FUNC_PLAY_PAUSE,  'p', 0 },
	{ "n",   "Next",     FUNC_NEXT,        'n', 0 },
	{ "m",   "P.Mode",   FUNC_PLAYMODE,    'm', 0 },
	{ "C",   "Clear",    FUNC_PL_CLEAR,    'C', 0 },
	{ "Del", "Remove",   FUNC_PL_DEL_ITEM, KEY_DC, 1 },
	{ "+",   "Vol+",     FUNC_VOLUME_UP,   '+', 0 },
	{ "-",   "Vol-",     FUNC_VOLUME_DOWN, '-', 0 },
	{ "/",   "Command",  FUNC_TEXT_INPUT,  '/', 0 },
	{ "q",   "Quit",     FUNC_QUIT,        'q', 0 },
	{ "t",   "Time",     FUNC_TOGGLE_TIME, 't', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_fb[] = {
	{ "Tab", "Window",   FUNC_NEXT_WINDOW, '\t', 0 },
	{ "a",   "Add",      FUNC_FB_ADD,      'a', 0 },
	{ "b",   "Prev",     FUNC_PREVIOUS,    'b', 0 },
	{ "S",   "Stop",     FUNC_STOP,        'S', 0 },
	{ "p",   "Pl/Pause", FUNC_PLAY_PAUSE,  'p', 0 },
	{ "n",   "Next",     FUNC_NEXT,        'n', 0 },
	{ "+",   "Vol+",     FUNC_VOLUME_UP,   '+', 0 },
	{ "-",   "Vol-",     FUNC_VOLUME_DOWN, '-', 0 },
	{ "/",   "Command",  FUNC_TEXT_INPUT,  '/', 0 },
	{ "q",   "Quit",     FUNC_QUIT,        'q', 0 },
	{ "t",   "Time",     FUNC_TOGGLE_TIME, 't', 0 },
	{ "A",   "Add to Mlib", FUNC_FB_MLIB_ADD_PATH, 'A', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_ti[] = {
	{ "Tab", "Window",   FUNC_NEXT_WINDOW, '\t', 0 },
	{ "b",   "Prev",     FUNC_PREVIOUS,    'b', 0 },
	{ "S",   "Stop",     FUNC_STOP,        'S', 0 },
	{ "p",   "Pl/Pause", FUNC_PLAY_PAUSE,  'p', 0 },
	{ "n",   "Next",     FUNC_NEXT,        'n', 0 },
	{ "+",   "Vol+",     FUNC_VOLUME_UP,   '+', 0 },
	{ "-",   "Vol-",     FUNC_VOLUME_DOWN, '-', 0 },
	{ "/",   "Command",  FUNC_TEXT_INPUT,  '/', 0 },
	{ "q",   "Quit",     FUNC_QUIT,        'q', 0 },
	{ "t",   "Time",     FUNC_TOGGLE_TIME, 't', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_cmd[] = {
	{ "Tab", "Window",   FUNC_NEXT_WINDOW, '\t', 0 },
	{ "b",   "Prev",     FUNC_PREVIOUS,    'b', 0 },
	{ "S",   "Stop",     FUNC_STOP,        'S', 0 },
	{ "p",   "Pl/Pause", FUNC_PLAY_PAUSE,  'p', 0 },
	{ "n",   "Next",     FUNC_NEXT,        'n', 0 },
	{ "+",   "Vol+",     FUNC_VOLUME_UP,   '+', 0 },
	{ "-",   "Vol-",     FUNC_VOLUME_DOWN, '-', 0 },
	{ "/",   "Command",  FUNC_TEXT_INPUT,  '/', 0 },
	{ "q",   "Quit",     FUNC_QUIT,        'q', 0 },
	{ "t",   "Time",     FUNC_TOGGLE_TIME, 't', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_lib[] = {
	{ "Tab", "Window",   FUNC_NEXT_WINDOW,    '\t', 0 },
	{ "b",   "Prev",     FUNC_PREVIOUS,       'b', 0 },
	{ "S",   "Stop",     FUNC_STOP,           'S', 0 },
	{ "p",   "Pl/Pause", FUNC_PLAY_PAUSE,     'p', 0 },
	{ "n",   "Next",     FUNC_NEXT,           'n', 0 },
	{ "a",   "Artists",  FUNC_BROWSE_ARTISTS, 'a', 0 },
	{ "g",   "Genres",   FUNC_BROWSE_GENRES,  'g', 0 },
	{ "s",   "Search",   FUNC_SEARCH,         's', 0 },
	{ "R",   "Refresh",  FUNC_MLIB_REFRESH,   'R', 0 },
	{ "i",   "Pl. item", FUNC_MLIB_PLAY_ITEM, 'i', 0 },
	{ "+",   "Vol+",     FUNC_VOLUME_UP,      '+', 0 },
	{ "-",   "Vol-",     FUNC_VOLUME_DOWN,    '-', 0 },
	{ "/",   "Command",  FUNC_TEXT_INPUT,     '/', 0 },
	{ "q",   "Quit",     FUNC_QUIT,           'q', 0 },
	{ "t",   "Time",     FUNC_TOGGLE_TIME,    't', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

void ui_update_trackinfo(UI *ui, char *title, char *artist, char *album, char *date)
{
	if (title) {
		strncpy(ui->ti_title, title, 128);
		ui->ti_title[127] = '\0';
	} else {
		memset(ui->ti_title, 0, 128);
	}
	if (artist) {
		strncpy(ui->ti_artist, artist, 128);
		ui->ti_artist[127] = '\0';
	} else {
		memset(ui->ti_artist, 0, 128);
	}
	if (album) {
		strncpy(ui->ti_album, album, 128);
		ui->ti_album[127] = '\0';
	} else {
		memset(ui->ti_album, 0, 128);
	}
	if (date) {
		strncpy(ui->ti_date, date, 64);
		ui->ti_date[63] = '\0';
	} else {
		memset(ui->ti_date, 0, 64);
	}
}

void ui_update_playlist_pos(UI *ui, int playlist_pos)
{
	ui->playlist_pos = playlist_pos;
}

void ui_update_playmode(UI *ui, int playmode)
{
	ui->playmode = playmode;
}

void ui_update_volume(UI *ui, int volume)
{
	ui->volume = volume;
}

void ui_update_playback_time(UI *ui, int time)
{
	ui->pb_time = time;
}

void ui_update_playback_status(UI *ui, char *status)
{
	strncpy(ui->status, status, 32);
	ui->ti_date[31] = '\0';
}

void ui_set_total_track_time(UI *ui, int seconds)
{
	ui->total_time = seconds;
}

void ui_draw_trackinfo(UI *ui)
{
	wattron(ui->win_ti->win, A_BOLD);
	mvwprintw(ui->win_ti->win, 0, 0, "Title:\n");
	wclrtoeol(ui->win_ti->win);
	wattroff(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, ui->ti_title);
	wclrtoeol(ui->win_ti->win);
	wattron(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, "\nArtist:\n");
	wclrtoeol(ui->win_ti->win);
	wattroff(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, ui->ti_artist);
	wclrtoeol(ui->win_ti->win);
	wattron(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, "\nAlbum:\n");
	wclrtoeol(ui->win_ti->win);
	wattroff(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, ui->ti_album);
	wclrtoeol(ui->win_ti->win);
	wattron(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, "\nDate:\n");
	wclrtoeol(ui->win_ti->win);
	wattroff(ui->win_ti->win, A_BOLD);
	wprintw(ui->win_ti->win, ui->ti_date);
	wclrtoeol(ui->win_ti->win);
	wclrtobot(ui->win_ti->win);
}

void ui_init(UI *ui, int color)
{
	memset(ui, 0, sizeof(UI));
	ui->color = color;
	ui->busy = 0;
	ui->time_display_remaining = 0;
	ui->total_time = 0;
	setlocale(LC_CTYPE, ""); /* Set system locale (which hopefully is a UTF-8 locale) */
	ui->rootwin = initscr();
	if (ui->color) {
		if (has_colors()) {
			start_color();
			use_default_colors();
			init_pair(1, COLOR_YELLOW, COLOR_BLACK);
			init_pair(2, COLOR_GREEN, COLOR_BLACK);
			init_pair(3, COLOR_BLUE, COLOR_BLACK);
			init_pair(4, COLOR_WHITE, COLOR_BLACK);
			init_pair(5, COLOR_BLACK, COLOR_WHITE);
		} else {
			ui->color = 0;
		}
	}
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	getmaxyx(stdscr, ui->rows, ui->cols);
	ui->win_cmd    = window_create(ui->rows-2, ui->cols, 1, 0, "Command window");
	ui->win_ti     = window_create(ui->rows-2, ui->cols, 1, 0, "Track information");
	ui->win_header = window_create(1, ui->cols, 0, 0, NULL);
	ui->win_footer = window_create(1, ui->cols, ui->rows-1, 0, NULL);
	ui->active_win = WIN_PL;
	ui->lw_fb      = listwidget_new(2, "File Browser", 1, 0, ui->rows-2, ui->cols);
	listwidget_set_col_width(ui->lw_fb, 0, 6);
	listwidget_set_col_width(ui->lw_fb, 1, -1);
	ui->lw_pl      = listwidget_new(3, "Playlist", 1, 0, ui->rows-2, ui->cols);
	listwidget_set_col_width(ui->lw_pl, 0, 6);
	listwidget_set_col_width(ui->lw_pl, 1, -1);
	listwidget_set_col_width(ui->lw_pl, 2, 6);
	ui->lw_mlib_search = listwidget_new(4, "Media Library Search", 1, 0, ui->rows-2, ui->cols);
	listwidget_set_col_width(ui->lw_mlib_search, 0, 30);
	listwidget_set_col_width(ui->lw_mlib_search, 1, 25);
	listwidget_set_col_width(ui->lw_mlib_search, 2, -1);
	listwidget_set_col_width(ui->lw_mlib_search, 3, 0);
	ui->lw_mlib_artists = listwidget_new(1, "Media Library: Artists", 1, 0, ui->rows-2, ui->cols);
	listwidget_set_col_width(ui->lw_mlib_artists, 0, -1);
	ui->lw_mlib_genres  = listwidget_new(1, "Media Library: Genres", 1, 0, ui->rows-2, ui->cols);
	listwidget_set_col_width(ui->lw_mlib_genres, 0, -1);
	ui->fb_pl = fb_pl;
	ui->fb_fb = fb_fb;
	ui->fb_ti = fb_ti;
	ui->fb_cmd = fb_cmd;
	ui->fb_lib = fb_lib;
	ui->fb_visible = fb_pl;
	ui->text_input_enabled = 0;
	ui->mlib_state = MLIB_STATE_BROWSE_ARTISTS;
}

void ui_draw_header(UI *ui)
{
	if (ui->win_header) {
		char pm1 = ' ', pm2 = ' ';
		int t;
		int min = 0, sec = 0;
		wattron(ui->win_header->win, A_BOLD);
		if (ui->color) wattron(ui->win_header->win, COLOR_PAIR(1));
		mvwprintw(ui->win_header->win, 0, 0, "Gmu");
		if (ui->color) wattroff(ui->win_header->win, COLOR_PAIR(1));
		if (ui->color) wbkgdset(ui->win_header->win, COLOR_PAIR(4));
		wclrtoeol(ui->win_header->win);
		wprintw(
			ui->win_header->win, " %s ",
			ui->status
		);
		if (ui->playlist_pos > 0 && ui->ti_title[0] != '\0') {
			if (ui->color) wattron(ui->win_header->win, COLOR_PAIR(2));
			wprintw(
				ui->win_header->win, "%d ",
				ui->playlist_pos
			);
			if (ui->color) wattroff(ui->win_header->win, COLOR_PAIR(2));
		}
		wprintw(
			ui->win_header->win, "%s%s%s",
			ui->ti_artist,
			ui->ti_artist[0] != '\0' ? " - " : "",
			ui->ti_title
		);
		t = ui->time_display_remaining ? ui->total_time - ui->pb_time / 1000 : ui->pb_time / 1000;
		min = t / 60;
		sec = t - min * 60;
		switch (ui->playmode) {
			case PM_CONTINUE:
				pm1 = 'C'; pm2 = ' ';
				break;
			case PM_REPEAT_ALL:
				pm1 = 'C'; pm2 = 'R';
				break;
			case PM_REPEAT_1:
				pm1 = 'C'; pm2 = '1';
				break;
			case PM_RANDOM:
				pm1 = 'R'; pm2 = ' ';
				break;
			case PM_RANDOM_REPEAT:
				pm1 = 'R'; pm2 = 'R';
				break;
		}
		wattroff(ui->win_header->win, A_BOLD);
		mvwprintw(ui->win_header->win, 0, ui->cols-15, "[");
		wattron(ui->win_header->win, A_BOLD);
		wprintw(ui->win_header->win, "%03d", ui->volume);
		wattroff(ui->win_header->win, A_BOLD);
		wprintw(ui->win_header->win, "|");
		wattron(ui->win_header->win, A_BOLD);
		wprintw(ui->win_header->win, "%c%c", pm1, pm2);
		wattroff(ui->win_header->win, A_BOLD);
		wprintw(ui->win_header->win, "]");
		wattron(ui->win_header->win, A_BOLD | A_BLINK);
		wprintw(ui->win_header->win, ui->busy ? "*" : " ");
		wattroff(ui->win_header->win, A_BOLD | A_BLINK);
		wattron(ui->win_header->win, A_BOLD);
		wprintw(ui->win_header->win,
		        min >= 0 && sec >= 0 ? "%3d:%02d" : " --:--",
		        min, sec);
		wattroff(ui->win_header->win, A_BOLD);
		window_refresh(ui->win_header);
	}
}

static void ui_draw_footer_button(UI *ui, char *key, char *name)
{
	if (ui->color) wattron(ui->win_footer->win, COLOR_PAIR(2));
	wattron(ui->win_footer->win, A_BOLD);
	wprintw(ui->win_footer->win, key);
	wattroff(ui->win_footer->win, A_BOLD);
	if (ui->color) wattroff(ui->win_footer->win, COLOR_PAIR(2));
	if (ui->color) wattron(ui->win_footer->win, COLOR_PAIR(4));
	wprintw(ui->win_footer->win, ":");
	wattron(ui->win_footer->win, A_BOLD);
	wprintw(ui->win_footer->win, "%s ", name);
	wattroff(ui->win_footer->win, A_BOLD);
	if (ui->color) wbkgdset(ui->win_footer->win, COLOR_PAIR(4));
	wclrtoeol(ui->win_header->win);
}

void ui_refresh_active_window(UI *ui)
{
	switch (ui->active_win) {
		default:
		case WIN_PL:
			listwidget_draw(ui->lw_pl);  
			listwidget_refresh(ui->lw_pl);
			break;
		case WIN_FB:
			listwidget_draw(ui->lw_fb);
			listwidget_refresh(ui->lw_fb);
			break;
		case WIN_TI:
			ui_draw_trackinfo(ui);
			window_refresh(ui->win_ti);
			break;
		case WIN_LIB:
			switch (ui->mlib_state) {
				case MLIB_STATE_RESULTS:
					listwidget_draw(ui->lw_mlib_search);
					listwidget_refresh(ui->lw_mlib_search);
					break;
				case MLIB_STATE_BROWSE_ARTISTS:
					listwidget_draw(ui->lw_mlib_artists);
					listwidget_refresh(ui->lw_mlib_artists);
					break;
				case MLIB_STATE_BROWSE_GENRES:
					listwidget_draw(ui->lw_mlib_genres);
					listwidget_refresh(ui->lw_mlib_genres);
					break;
			}
			break;
		case WIN_CMD:
			window_refresh(ui->win_cmd);
			break;
	}
}

void ui_draw_footer(UI *ui)
{
	if (ui->win_footer) {
		int i;
		getmaxyx(stdscr, ui->rows, ui->cols);
		wmove(ui->win_footer->win, 0, 0);
		for (i = 0; ui->fb_visible && ui->fb_visible[i].button_name; i++) {
			int __attribute__((unused)) y;
			int x;
			getyx(ui->win_footer->win, y, x);
			if (x < ui->cols-2)
				ui_draw_footer_button(ui, ui->fb_visible[i].button_name, ui->fb_visible[i].button_desc);
		}
		wclrtoeol(ui->win_footer->win);
	}
	window_refresh(ui->win_footer);
}

void ui_draw(UI *ui)
{
	clear();
	refresh();

	ui_draw_header(ui);
	ui_draw_footer(ui);

	ui_refresh_active_window(ui);
	move(ui->rows-2, 1);
}

void ui_cursor_text_input(UI *ui, char *str)
{
	if (ui->text_input_enabled) {
		wmove(ui->win_footer->win, 0, 0);
		wclrtoeol(ui->win_footer->win);
		wprintw(ui->win_footer->win, ">%s", str ? str : "");
		wrefresh(ui->win_footer->win);
	}
}

void ui_enable_text_input(UI *ui, int enable)
{
	ui->text_input_enabled = enable;
	curs_set(enable);
}

void ui_resize(UI *ui)
{
	clear();
	endwin();
	doupdate();
	getmaxyx(stdscr, ui->rows, ui->cols);
	window_resize(ui->win_cmd, ui->rows-2, ui->cols);
	window_resize(ui->win_ti, ui->rows-2, ui->cols);
	listwidget_resize(ui->lw_pl, ui->rows-2, ui->cols);
	listwidget_resize(ui->lw_fb, ui->rows-2, ui->cols);
	listwidget_resize(ui->lw_mlib_search, ui->rows-2, ui->cols);
	listwidget_resize(ui->lw_mlib_artists, ui->rows-2, ui->cols);
	listwidget_resize(ui->lw_mlib_genres, ui->rows-2, ui->cols);
	mvwin(ui->win_footer->win, ui->rows-1, 0);
	ui_draw(ui);
	if (ui->win_cmd && ui->win_cmd->win)
		wprintw(ui->win_cmd->win, "new size: %d x %d\n", ui->cols, ui->rows);
	ui_refresh_active_window(ui);
}

void ui_set_footer_buttons(UI *ui)
{
	FooterButtons *fb = NULL;
	switch (ui->active_win) {
		default:
		case WIN_PL:  fb = ui->fb_pl; break;
		case WIN_FB:  fb = ui->fb_fb; break;
		case WIN_TI:  fb = ui->fb_ti; break;
		case WIN_LIB: fb = ui->fb_lib; break;
		case WIN_CMD: fb = ui->fb_cmd; break;
	}
	if (fb != ui->fb_visible) {
		ui->fb_visible = fb;
		ui_draw_footer(ui);
	}
}

void ui_active_win_next(UI *ui)
{
	ui->active_win++;
	if (ui->active_win > WIN_LIB)
#ifdef DEBUG
		ui->active_win = WIN_CMD;
#else
		ui->active_win = WIN_PL;
#endif
	ui_set_footer_buttons(ui);
}

void ui_active_win_set(UI *ui, WindowType aw)
{
	ui->active_win = aw;
	ui_set_footer_buttons(ui);
}

void ui_free(UI *ui)
{
	window_destroy(ui->win_cmd);
	window_destroy(ui->win_ti);
	window_destroy(ui->win_header);
	window_destroy(ui->win_footer);
	listwidget_free(ui->lw_pl);
	listwidget_free(ui->lw_fb);
	listwidget_free(ui->lw_mlib_search);
	listwidget_free(ui->lw_mlib_artists);
	listwidget_free(ui->lw_mlib_genres);
	endwin();
}

int ui_has_color(UI *ui)
{
	return ui->color;
}

void ui_busy_indicator(UI *ui, int busy)
{
	if (busy)
		ui->busy++;
	else
		ui->busy--;
}

void ui_toggle_time_display(UI *ui)
{
	ui->time_display_remaining = !ui->time_display_remaining;
}

void ui_mlib_set_state(UI *ui, MLibState state)
{
	ui->mlib_state = state;
}

MLibState ui_mlib_get_state(UI *ui)
{
	return ui->mlib_state;
}

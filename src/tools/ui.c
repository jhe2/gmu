/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
	{ "F1",  "Help",     FUNC_HELP,        KEY_F(1) },
	{ "F2",  "Window",   FUNC_NEXT_WINDOW, KEY_F(2) },
	{ "F3",  "Search",   FUNC_SEARCH,      KEY_F(3) },
	{ "F5",  "Prev",     FUNC_PREVIOUS,    KEY_F(5) },
	{ "F6",  "Stop",     FUNC_STOP,        KEY_F(6) },
	{ "F7",  "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7) },
	{ "F8",  "Next",     FUNC_NEXT,        KEY_F(8) },
	{ "F9",  "P.Mode",   FUNC_PLAYMODE,    KEY_F(9) },
	{ "F10", "Clear",    FUNC_PL_CLEAR,    KEY_F(10) },
	{ "Del", "Remove",   FUNC_PL_DEL_ITEM, KEY_DC },
	{ NULL, NULL, FUNC_NONE, 0 }
};

static FooterButtons fb_fb[] = {
	{ "F1", "Help",     FUNC_HELP,        KEY_F(1) },
	{ "F2", "Window",   FUNC_NEXT_WINDOW, KEY_F(2) },
	{ "F3", "Add",      FUNC_FB_ADD,      KEY_F(3) },
	{ "F5", "Prev",     FUNC_PREVIOUS,    KEY_F(5) },
	{ "F6", "Stop",     FUNC_STOP,        KEY_F(6) },
	{ "F7", "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7) },
	{ "F8", "Next",     FUNC_NEXT,        KEY_F(8) },
	{ NULL, NULL, FUNC_NONE, 0 }
};

static FooterButtons fb_ti[] = {
	{ "F1", "Help",     FUNC_HELP,        KEY_F(1) },
	{ "F2", "Window",   FUNC_NEXT_WINDOW, KEY_F(2) },
	{ "F5", "Prev",     FUNC_PREVIOUS,    KEY_F(5) },
	{ "F6", "Stop",     FUNC_STOP,        KEY_F(6) },
	{ "F7", "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7) },
	{ "F8", "Next",     FUNC_NEXT,        KEY_F(8) },
	{ NULL, NULL, FUNC_NONE, 0 }
};

static FooterButtons fb_cmd[] = {
	{ "F1", "Help",     FUNC_HELP,        KEY_F(1) },
	{ "F2", "Window",   FUNC_NEXT_WINDOW, KEY_F(2) },
	{ "F5", "Prev",     FUNC_PREVIOUS,    KEY_F(5) },
	{ "F6", "Stop",     FUNC_STOP,        KEY_F(6) },
	{ "F7", "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7) },
	{ "F8", "Next",     FUNC_NEXT,        KEY_F(8) },
	{ NULL, NULL, FUNC_NONE, 0 }
};

void ui_init(UI *ui)
{
	setlocale(LC_CTYPE, ""); /* Set system locale (which hopefully is a UTF-8 locale) */
	ui->rootwin = initscr();
	//start_color();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	getmaxyx(stdscr, ui->rows, ui->cols);
	ui->win_cmd    = window_create(ui->rows-3, ui->cols, 1, 0, "Command window");
	ui->win_ti     = window_create(ui->rows-3, ui->cols, 1, 0, "Track information");
	ui->win_header = window_create(1, ui->cols, 0, 0, NULL);
	ui->win_footer = window_create(2, ui->cols, ui->rows-2, 0, NULL);
	ui->active_win = WIN_PL;
	ui->lw_fb      = listwidget_new(2, "File Browser", 1, 0, ui->rows-3, ui->cols);
	listwidget_set_col_width(ui->lw_fb, 0, 6);
	listwidget_set_col_width(ui->lw_fb, 1, -1);
	ui->lw_pl      = listwidget_new(3, "Playlist", 1, 0, ui->rows-3, ui->cols);
	listwidget_set_col_width(ui->lw_pl, 0, 6);
	listwidget_set_col_width(ui->lw_pl, 1, -1);
	listwidget_set_col_width(ui->lw_pl, 2, 6);
	ui->fb_pl = fb_pl;
	ui->fb_fb = fb_fb;
	ui->fb_ti = fb_ti;
	ui->fb_cmd = fb_cmd;
	ui->fb_visible = fb_pl;
}

void ui_draw_header(UI *ui, char *cur_artist, char *cur_title, 
                    char *cur_status, int cur_time, int playmode)
{
	if (ui->win_header) {
		char pm1 = ' ', pm2 = ' ';
		int min = 0, sec = 0;
		wattron(ui->win_header->win, A_BOLD);
		mvwprintw(ui->win_header->win, 0, 0, "Gmu");
		wclrtoeol(ui->win_header->win);
		wattroff(ui->win_header->win, A_BOLD);
		wprintw(ui->win_header->win, " %s %s%s%s",
		        cur_status, 
		        cur_artist != NULL ? cur_artist : "", 
		        cur_artist != NULL ? " - " : "", cur_title);
		min = (cur_time / 1000) / 60;
		sec = (cur_time / 1000) - min * 60;
		switch (playmode) {
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
		mvwprintw(ui->win_header->win, 0, ui->cols-9, "%c%c %3d:%02d", pm1, pm2, min, sec);
		window_refresh(ui->win_header);
	}
}

static void ui_draw_footer_button(UI *ui, char *key, char *name)
{
	char btn_str[8] = "       ";
	int  len = strlen(name);
	if (len > 7) len = 7;
	wattron(ui->win_footer->win, A_BOLD);
	wprintw(ui->win_footer->win, key);
	wattroff(ui->win_footer->win, A_BOLD);
	wattron(ui->win_footer->win, A_REVERSE);
	memcpy(btn_str, name, len);
	wprintw(ui->win_footer->win, "%s", btn_str);
	wattroff(ui->win_footer->win, A_REVERSE);
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
			window_refresh(ui->win_ti);
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
		wmove(ui->win_footer->win, 1, 0);
		for (i = 0; ui->fb_visible && ui->fb_visible[i].button_name; i++)
			ui_draw_footer_button(ui, ui->fb_visible[i].button_name, ui->fb_visible[i].button_desc);
		wclrtoeol(ui->win_footer->win);
	}
	window_refresh(ui->win_footer);
}

void ui_draw(UI *ui)
{
	clear();
	refresh();

	ui_draw_header(ui, NULL, NULL, NULL, 0, 0);
	ui_draw_footer(ui);

	ui_refresh_active_window(ui);
	move(ui->rows-2, 1);
}

void ui_cursor_text_input(UI *ui, char *str)
{
	wmove(ui->win_footer->win, 0, 0);
	wclrtoeol(ui->win_footer->win);
	wprintw(ui->win_footer->win, ">%s", str ? str : "");
	wrefresh(ui->win_footer->win);
}

void ui_resize(UI *ui)
{
	clear();
	endwin();
	doupdate();
	getmaxyx(stdscr, ui->rows, ui->cols);
	window_resize(ui->win_cmd, ui->rows-3, ui->cols);
	listwidget_resize(ui->lw_pl, ui->rows-3, ui->cols);
	listwidget_resize(ui->lw_fb, ui->rows-3, ui->cols);
	mvwin(ui->win_footer->win, ui->rows-2, 0);
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
	if (ui->active_win > WIN_TI) ui->active_win = WIN_CMD;
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
	endwin();
}

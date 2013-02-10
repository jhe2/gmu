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
	{ "Tab", "Window",   FUNC_NEXT_WINDOW, '\t', 0 },
	{ "F3",  "Search",   FUNC_SEARCH,      KEY_F(3), 1 },
	{ "F5",  "Prev",     FUNC_PREVIOUS,    KEY_F(5), 1 },
	{ "F6",  "Stop",     FUNC_STOP,        KEY_F(6), 1 },
	{ "F7",  "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7), 1 },
	{ "F8",  "Next",     FUNC_NEXT,        KEY_F(8), 1 },
	{ "F9",  "P.Mode",   FUNC_PLAYMODE,    KEY_F(9), 1 },
	{ "F10", "Clear",    FUNC_PL_CLEAR,    KEY_F(10), 1 },
	{ "Del", "Remove",   FUNC_PL_DEL_ITEM, KEY_DC, 1 },
	{ "/", "Command",    FUNC_TEXT_INPUT,  '/', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_fb[] = {
	{ "Tab", "Window",  FUNC_NEXT_WINDOW, '\t', 0 },
	{ "F3", "Add",      FUNC_FB_ADD,      KEY_F(3), 1 },
	{ "F5", "Prev",     FUNC_PREVIOUS,    KEY_F(5), 1 },
	{ "F6", "Stop",     FUNC_STOP,        KEY_F(6), 1 },
	{ "F7", "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7), 1 },
	{ "F8", "Next",     FUNC_NEXT,        KEY_F(8), 1 },
	{ "/", "Command",   FUNC_TEXT_INPUT,  '/', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_ti[] = {
	{ "Tab", "Window",  FUNC_NEXT_WINDOW, '\t', 0 },
	{ "F5", "Prev",     FUNC_PREVIOUS,    KEY_F(5), 1 },
	{ "F6", "Stop",     FUNC_STOP,        KEY_F(6), 1 },
	{ "F7", "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7), 1 },
	{ "F8", "Next",     FUNC_NEXT,        KEY_F(8), 1 },
	{ "/", "Command",   FUNC_TEXT_INPUT,  '/', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

static FooterButtons fb_cmd[] = {
	{ "Tab", "Window",  FUNC_NEXT_WINDOW, '\t', 0 },
	{ "F5", "Prev",     FUNC_PREVIOUS,    KEY_F(5), 1 },
	{ "F6", "Stop",     FUNC_STOP,        KEY_F(6), 1 },
	{ "F7", "Pl/Pause", FUNC_PLAY_PAUSE,  KEY_F(7), 1 },
	{ "F8", "Next",     FUNC_NEXT,        KEY_F(8), 1 },
	{ "/", "Command",   FUNC_TEXT_INPUT,  '/', 0 },
	{ NULL, NULL, FUNC_NONE, 0, 0 }
};

void ui_init(UI *ui, int color)
{
	ui->color = color;
	setlocale(LC_CTYPE, ""); /* Set system locale (which hopefully is a UTF-8 locale) */
	ui->rootwin = initscr();
	if (ui->color) {
		if (has_colors()) {
			start_color();
			init_pair(1, COLOR_YELLOW, COLOR_BLACK);
			init_pair(2, COLOR_GREEN, COLOR_BLACK);
			init_pair(3, COLOR_BLUE, COLOR_BLACK);
			init_pair(4, COLOR_WHITE, COLOR_BLACK);
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
	ui->fb_pl = fb_pl;
	ui->fb_fb = fb_fb;
	ui->fb_ti = fb_ti;
	ui->fb_cmd = fb_cmd;
	ui->fb_visible = fb_pl;
	ui->text_input_enabled = 0;
}

void ui_draw_header(UI *ui, char *cur_artist, char *cur_title, 
                    char *cur_status, int cur_time, int playmode)
{
	if (ui->win_header) {
		char pm1 = ' ', pm2 = ' ';
		int min = 0, sec = 0;
		wattron(ui->win_header->win, A_BOLD);
		if (ui->color) wattron(ui->win_header->win, COLOR_PAIR(1));
		mvwprintw(ui->win_header->win, 0, 0, "Gmu");
		if (ui->color) wattroff(ui->win_header->win, COLOR_PAIR(1));
		wclrtoeol(ui->win_header->win);
		wprintw(ui->win_header->win, " %s %s%s%s",
		        cur_status != NULL ? cur_status : "", 
		        cur_artist != NULL ? cur_artist : "", 
		        cur_artist && cur_artist[0] != '\0' ? " - " : "",
		        cur_title != NULL ? cur_title : "");
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
		mvwprintw(ui->win_header->win, 0, ui->cols-11, "[%c%c] %3d:%02d", pm1, pm2, min, sec);
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
	if (ui->color) wattron(ui->win_footer->win, COLOR_PAIR(4));
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
		wmove(ui->win_footer->win, 0, 0);
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
	listwidget_resize(ui->lw_pl, ui->rows-2, ui->cols);
	listwidget_resize(ui->lw_fb, ui->rows-2, ui->cols);
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

int ui_has_color(UI *ui)
{
	return ui->color;
}

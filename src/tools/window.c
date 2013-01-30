/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: window.c  Created: 121028
 *
 * Description: Window widget for Ncurses Gmu text client
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdlib.h>
#include <string.h>
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <curses.h>
#include "window.h"

Window *window_create(int height, int width, int starty, int startx, char *title)
{
	Window *win = malloc(sizeof(Window));

	if (win) {
		win->width = 0;
		win->height = 0;
		win->title = NULL;
		if (title) { /* Only windows with title have a border */
			win->win_outer = newwin(height, width, starty, startx);
			if (win->win_outer) {
				win->width = width;
				win->height = height;
				scrollok(win->win_outer, 1);
				idlok(win->win_outer, 1); /* enable hardware scrolling (if available) */
				win->win = derwin(win->win_outer, height-2, width-2, 1, 1);
				box(win->win_outer, 0, 0); /* 0, 0 gives default characters 
											* for the vertical and horizontal
											* lines                           */
				window_update_title(win, title);
				if (win->win) {
					scrollok(win->win, 1);
					idlok(win->win, 1); /* enable hardware scrolling (if available) */
					wsetscrreg(win->win, 1, height-2);
				}
			}
		} else { /* Simple borderless window */
			win->win = newwin(height, width, starty, startx);
			win->win_outer = NULL;
		}
	}
	return win;
}

int window_update_title(Window *win, char *title)
{
	int len = strlen(title), res = 0;
	win->title = realloc(win->title, len+1);
	if (win->title) {
		strncpy(win->title, title, len);
		win->title[len] = '\0';
		mvwprintw(win->win_outer, 0, 1, " %s ", win->title);
		res = 1;
	}
	return res;
}

void window_resize(Window *win, int height, int width)
{
	if (win && win->win) {
		if (!win->win_outer || wresize(win->win_outer, height, width) == OK) {
			int ok = 0;
			int d = win->win_outer ? 2 : 0;
			if (win->win_outer) {
				delwin(win->win);
				win->win = derwin(win->win_outer, height-2, width-2, 1, 1);
				wclear(win->win);
				scrollok(win->win_outer, 1);
				idlok(win->win_outer, 1);
				ok = 1;
			} else if (wresize(win->win, height-d, width-d) == OK) {
				ok = 1;
			}
			if (ok) {
				win->width = width;
				win->height = height;
				scrollok(win->win, 1);
				idlok(win->win, 1);
				wsetscrreg(win->win, 1, height-2);
				if (win->title) {
					box(win->win_outer, 0, 0);
					mvwprintw(win->win_outer, 0, 1, " %s ", win->title);
				}
			}
		} else { endwin(); exit(-10); }
	}
}

int window_get_width(Window *win)
{
	return win->width;
}

int window_get_height(Window *win)
{
	return win->height;
}

int window_refresh(Window *win)
{
	int res = 0;
	if (win && win->win) {
		if (win->win_outer) {
			redrawwin(win->win_outer);
			wrefresh(win->win_outer);
		} else {
			redrawwin(win->win);
			wrefresh(win->win);
		}
		res = 1;
	}
	return res;
}

void window_destroy(Window *win)
{	
	wborder(win->win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	/* The parameters taken are 
	 * 1. win: the window on which to operate
	 * 2. ls: character to be used for the left side of the window 
	 * 3. rs: character to be used for the right side of the window 
	 * 4. ts: character to be used for the top side of the window 
	 * 5. bs: character to be used for the bottom side of the window 
	 * 6. tl: character to be used for the top left corner of the window 
	 * 7. tr: character to be used for the top right corner of the window 
	 * 8. bl: character to be used for the bottom left corner of the window 
	 * 9. br: character to be used for the bottom right corner of the window
	 */
	wrefresh(win->win);
	delwin(win->win);
	if (win->title) free(win->title);
	free(win);
}

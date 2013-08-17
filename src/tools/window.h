/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: window.h  Created: 121028
 *
 * Description: Window widget for Ncurses Gmu text client
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef WEJ_WINDOW_H
#define WEJ_WINDOW_H
typedef enum WindowType { WIN_CMD, WIN_PL, WIN_TI, WIN_FB, WIN_LIB } WindowType;

typedef struct Window
{
	WINDOW *win, *win_outer;
	char   *title;
	int     width, height;
} Window;

Window *window_create(int height, int width, int starty, int startx, char *title);
int     window_update_title(Window *win, char *title);
void    window_resize(Window *win, int height, int width);
int     window_get_width(Window *win);
int     window_get_height(Window *win);
int     window_refresh(Window *win);
void    window_destroy(Window *win);
#endif

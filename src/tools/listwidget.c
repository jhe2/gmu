/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: listwidget.h  Created: 121027
 *
 * Description: ListWidget for Ncurses Gmu text client
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
#include "listwidget.h"

ListWidget *listwidget_new(int cols, char *title, int y, int x, int height, int width)
{
	ListWidget *lw = malloc(sizeof(ListWidget));
	if (lw) {
		lw->cols = cols;
		lw->rows = 0;
		lw->first = NULL;
		lw->last = NULL;
		lw->cursor_pos = -1;
		lw->first_visible_row = 0;
		lw->pos_marker = 0;
		lw->win = window_create(height, width, y, x, title);
		lw->col_width = malloc(sizeof(int) * cols);
	}
	return lw;
}

int listwidget_set_col_width(ListWidget *lw, int col, int width)
{
	int res = 0;
	if (col < lw->cols) {
		lw->col_width[col] = width;
		res = 1;
	}
	return res;
}

int listwidget_get_selection(ListWidget *lw)
{
	return lw->cursor_pos;
}

int listwidget_get_rows(ListWidget *lw)
{
	return lw->rows;
}

char *listwidget_get_row_data(ListWidget *lw, int row, int col)
{
	int i;
	ListCell *crow = lw->first;
	for (i = 0; i < row && crow; i++) {
		crow = crow->next_row;
	}
	for (i = 0; i < col && crow; i++) {
		crow = crow->next_column;
	}
	return crow ? crow->text : NULL;
}

int listwidget_delete_row(ListWidget *lw, int row)
{
	int res = 1, i;
	ListCell *pr = NULL, *cr = lw->first, *nr = NULL;
	for (i = 0; i < row && cr; i++) {
		pr = cr;
		cr = cr->next_row;
	}
	if (cr) {
		ListCell *ccol = cr, *ncol;
		nr = cr->next_row;
		while (ccol) {
			ncol = ccol->next_column;
			free(ccol);
			ccol = ncol;
		}
		lw->rows--;
	}
	if (pr) pr->next_row = nr; else lw->first = nr;
	if (nr) nr->prev_row = pr;
	return res;
}

static ListCell *new_cell(void)
{
	ListCell *lc = malloc(sizeof(ListCell));
	if (lc) {
		lc->next_row = NULL;
		lc->prev_row = NULL;
		lc->next_column = NULL;
		lc->text[0] = '\0';
	}
	return lc;
}

static ListCell *new_row(int cols)
{
	ListCell *lc = new_cell(), *tmp;
	int       i;
	if (lc) {
		tmp = lc;
		for (i = 0; i < cols-1; i++) {
			tmp->next_column = new_cell();
			tmp = tmp->next_column;
		}
	}
	return lc;
}

int listwidget_add_row(ListWidget *lw)
{
	int res = 0;
	ListCell *lr = new_row(lw->cols);
	if (lr) {
		if (!lw->first) {
			lw->first = lr;
			lw->cursor_pos = 0;
		} else {
			lw->last->next_row = lr;
		}
		lr->prev_row = lw->last;
		lr->next_row = NULL;
		lw->last = lr;
		lw->rows++;
		res = 1;
	}
	return res ? lw->rows : -1;
}

int listwidget_insert_row(ListWidget *lw, int after_row)
{
	return 0;
}

int listwidget_set_cell_data(ListWidget *lw, int row, int col, char *str)
{
	int i, res = 0;
	if (row < lw->rows) {
		ListCell *rowc = lw->first;
		for (i = 0; i < row && rowc; i++)
			rowc = rowc->next_row;
		if (rowc) {
			ListCell *colc = rowc;
			for (i = 0; i < col && colc; i++)
				colc = colc->next_column;
			if (colc) {
				strncpy(colc->text, str, LW_MAX_TEXT_LENGTH);
				colc->text[LW_MAX_TEXT_LENGTH] = '\0';
			}
		}
		if (row >= lw->first_visible_row && row < lw->first_visible_row+lw->win->height-2)
			listwidget_draw(lw);
	}
	return res;
}

int listwidget_clear_all_rows(ListWidget *lw)
{
	return listwidget_set_length(lw, 0);
}

int listwidget_draw(ListWidget *lw)
{
	int       row, col, new_pos;
	ListCell *lc = lw->first;

	for (row = 0; row < lw->first_visible_row && lc; row++) {
		lc = lc->next_row;
	}
	for (row = lw->first_visible_row; row - lw->first_visible_row < lw->win->height && lc; row++) {
		ListCell *lrc = lc;
		int       col_pos = 0;
		if (row == lw->cursor_pos) wattron(lw->win->win, A_BOLD);
		for (col = 0; col < lw->cols && lrc; col++) {
			mvwprintw(lw->win->win, row - lw->first_visible_row, col_pos, lrc && lrc->text[0] != '\0' ? lrc->text : "?");
			wclrtoeol(lw->win->win);
			if (lw->col_width[col] > 0) { /* Normal column width */
				col_pos += lw->col_width[col];
			} else { /* negative column width = fill available space */
				int i;
				col_pos = lw->win->width-2;
				for (i = col+1; i < lw->cols; i++) {
					col_pos -= lw->col_width[i];
				}
			}
			lrc = lrc->next_column;
		}
		if (row == lw->cursor_pos) wattroff(lw->win->win, A_BOLD);
		lc = lc->next_row;
	}
	for (; row < lw->win->height; row++) {
		wmove(lw->win->win, row, 0);
		wclrtoeol(lw->win->win);
	}

	new_pos = (lw->win->height-4) * ((float)lw->cursor_pos / lw->rows);
	if (new_pos != lw->pos_marker) {
		mvwvline_set(lw->win->win_outer, lw->pos_marker+2, lw->win->width-1, WACS_VLINE, 1);
		lw->pos_marker = new_pos;
		mvwadd_wchnstr(lw->win->win_outer, lw->pos_marker+2, lw->win->width-1, WACS_DIAMOND, 1);
	}

	if (lw->first_visible_row > 0)
		wattron(lw->win->win_outer, A_BOLD);
	mvwadd_wchnstr(lw->win->win_outer, 1, lw->win->width-1, WACS_UARROW, 1);
	if (lw->first_visible_row > 0)
		wattroff(lw->win->win_outer, A_BOLD);
	if (lw->first_visible_row + lw->win->height-2 < lw->rows)
		wattron(lw->win->win_outer, A_BOLD);
	mvwadd_wchnstr(lw->win->win_outer, lw->win->height-2, lw->win->width-1, WACS_DARROW, 1);
	if (lw->first_visible_row + lw->win->height-2 < lw->rows)
		wattroff(lw->win->win_outer, A_BOLD);
	return 0;
}

void listwidget_refresh(ListWidget *lw)
{
	window_refresh(lw->win);
}

int listwidget_move_cursor(ListWidget *lw, int offset)
{
	int res = 0;
	if (lw->cursor_pos + offset >= 0 && lw->cursor_pos + offset < lw->rows) {
		lw->cursor_pos += offset;
		/* Adjust first_visible_row if neccessary... */
		if (lw->cursor_pos < lw->first_visible_row)
			lw->first_visible_row = lw->cursor_pos;
		else if (lw->cursor_pos > lw->first_visible_row + (lw->win->height-2)-1)
			lw->first_visible_row = lw->cursor_pos - (lw->win->height-2)+1;
		res = 1;
	}
	return res;
}

int listwidget_set_cursor(ListWidget *lw, int pos)
{
	int res = 0;
	if (pos < lw->rows) {
		lw->cursor_pos = pos;
		/* Adjust first_visible_row if neccessary... */
		if (lw->cursor_pos < lw->first_visible_row)
			lw->first_visible_row = lw->cursor_pos;
		else if (lw->cursor_pos > lw->first_visible_row + lw->win->height)
			lw->first_visible_row = lw->cursor_pos + lw->win->height - 1;
		res = 1;
	}
	return res;
}

void listwidget_resize(ListWidget *lw, int height, int width)
{
	window_resize(lw->win, height, width);
}

int listwidget_set_length(ListWidget *lw, int rows)
{
	if (rows > lw->rows) {
		int i, rowcnt = lw->rows;
		for (i = 0; i < rows - rowcnt; i++) {
			listwidget_add_row(lw);
		}
	} else {
		int i, rowcnt = lw->rows;
		for (i = rows; i < rowcnt; i++) {
			listwidget_delete_row(lw, rows);
		}
	}
	if (lw->cursor_pos > rows-1) {
		lw->cursor_pos = rows-1;
		if (lw->cursor_pos < 0) lw->cursor_pos = 0;
		lw->first_visible_row = lw->cursor_pos;
	}
	lw->rows = rows;
	return 0;
}

int listwidget_free(ListWidget *lw)
{
	listwidget_set_length(lw, 0);
	if (lw->col_width) free(lw->col_width);
	window_destroy(lw->win);
	free(lw);
	return 0;
}

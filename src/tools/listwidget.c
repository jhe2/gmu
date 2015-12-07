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

static int internal_listwidget_set_length(ListWidget *lw, int rows)
{
	if (rows > 0) {
		lw->rows_ref = realloc(lw->rows_ref, sizeof(ListCell *) * rows);
		if (lw->rows_ref) lw->rows = rows; else lw->rows = 0;
	}
	return lw->rows_ref ? 1 : 0;
}

ListWidget *listwidget_new(int cols, char *title, int y, int x, int height, int width)
{
	ListWidget *lw = malloc(sizeof(ListWidget));
	if (lw) {
		lw->cols = cols;
		lw->rows = 0;
		lw->rows_ref = NULL;
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
	ListCell *crow = lw->rows_ref ? lw->rows_ref[row] : NULL;
	for (i = 0; i < col && crow; i++) {
		crow = crow->next_column;
	}
	return crow ? crow->text : NULL;
}

static void internal_free_row_memory(ListWidget *lw, ListCell *lc)
{
	int i;
	for (i = 0; i < lw->cols; i++) {
		ListCell *tmp = lc ? lc->next_column : NULL;
		free(lc);
		lc = tmp;
	}
}

int listwidget_delete_row(ListWidget *lw, int row)
{
	int res = 1, i;
	internal_free_row_memory(lw, lw->rows_ref[row]);
	for (i = row; i < lw->rows-1; i++)
		lw->rows_ref[i] = lw->rows_ref[i+1];

	lw->rows--;
	lw->rows_ref = realloc(lw->rows_ref, sizeof(ListCell *) * lw->rows);
	return res;
}

static ListCell *new_cell(void)
{
	ListCell *lc = malloc(sizeof(ListCell));
	if (lc) memset(lc, 0, sizeof(ListCell));
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
		if (internal_listwidget_set_length(lw, lw->rows+1)) {
			lw->rows_ref[lw->rows-1] = lr;
		} else {
			internal_free_row_memory(lw, lr);
		}
		if (lw->cursor_pos < 0) lw->cursor_pos = 0;
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
		ListCell *rowc = lw->rows_ref[row];
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

static void clear_row(ListWidget *lw, size_t row)
{
	char spaces[256];
	size_t w = lw->win->width - 2 < 255 ? lw->win->width - 2 : 255;
	memset(spaces, ' ', w);
	spaces[w] = '\0';
	mvwaddstr(lw->win->win, row, 0, spaces);
}

int listwidget_draw(ListWidget *lw)
{
	int row = 0, col, new_pos;

	if (lw->rows_ref && lw->first_visible_row >= 0) {
		for (row = lw->first_visible_row; row - lw->first_visible_row < lw->win->height && row < lw->rows; row++) {
			ListCell *lrc = lw->rows_ref[row];
			int       col_pos = 0;

			if (row == lw->cursor_pos) wattron(lw->win->win, A_REVERSE);
			clear_row(lw, row - lw->first_visible_row);

			for (col = 0; col < lw->cols && lrc; col++) {
				int col_w = lw->col_width[col];
				if (col_w < 0) {
					int i;
					col_w = lw->win->width-2;
					for (i = 0; i < lw->cols; i++)
						if (lw->col_width[i] > 0) col_w -= lw->col_width[i];
				}
				if (col_w > 1) {
					mvwaddnstr(
						lw->win->win,
						row - lw->first_visible_row,
						col_pos,
						lrc && lrc->text[0] != '\0' ? lrc->text : "",
						col_w - 1
					);
				}
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
			if (row == lw->cursor_pos) wattroff(lw->win->win, A_REVERSE);
		}
	}
	/* Blank remaining rows (if any) */
	for (; row - lw->first_visible_row < lw->win->height; row++) {
		clear_row(lw, row - lw->first_visible_row);
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
		res = 1;
	} else if (lw->cursor_pos + offset < 0) {
		lw->cursor_pos = 0;
		res = 1;
	} else if (lw->cursor_pos + offset >= lw->rows) {
		lw->cursor_pos = lw->rows - 1;
		res = 1;
	}
	if (res) {
		/* Adjust first_visible_row if neccessary... */
		if (lw->cursor_pos < lw->first_visible_row)
			lw->first_visible_row = lw->cursor_pos;
		else if (lw->cursor_pos > lw->first_visible_row + (lw->win->height-2)-1)
			lw->first_visible_row = lw->cursor_pos - (lw->win->height-2)+1;
	}
	return res;
}

int listwidget_set_cursor(ListWidget *lw, int pos)
{
	int res = 0;
	if (pos < lw->rows && (pos >= 0 || pos == LW_CURSOR_POS_END)) {
		if (pos == LW_CURSOR_POS_END)
			pos = listwidget_get_rows(lw) - 1;
		lw->cursor_pos = pos;
		/* Adjust first_visible_row if neccessary... */
		if (lw->cursor_pos < lw->first_visible_row)
			lw->first_visible_row = lw->cursor_pos;
		else if (lw->cursor_pos > lw->first_visible_row + lw->win->height - 3)
			lw->first_visible_row = lw->cursor_pos - lw->win->height + 3;
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
	int old_length = lw->rows;
	if (rows < old_length) {
		int i;
		for (i = old_length-1; i >= rows; i--) {
			listwidget_delete_row(lw, i);
		}
	}
	if (internal_listwidget_set_length(lw, rows)) {
		if (rows > old_length) {
			int i;
			for (i = old_length; i < rows; i++) {
				ListCell *lr = new_row(lw->cols);
				lw->rows_ref[i] = lr;
			}
		}
	}
	if (rows > 0 && lw->cursor_pos < 0) {
		lw->cursor_pos = 0;
	} else if (lw->cursor_pos > rows-1) {
		lw->cursor_pos = rows-1;
		if (lw->cursor_pos < 0) lw->cursor_pos = 0;
	}
	if (lw->first_visible_row > rows) lw->first_visible_row = rows;
	return 0;
}

int listwidget_free(ListWidget *lw)
{
	listwidget_set_length(lw, 0);
	if (lw->rows_ref) free(lw->rows_ref);
	if (lw->col_width) free(lw->col_width);
	window_destroy(lw->win);
	free(lw);
	return 0;
}

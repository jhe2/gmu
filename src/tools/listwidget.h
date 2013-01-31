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

#ifndef WEJ_LISTWIDGET_H
#define WEJ_LISTWIDGET_H
typedef struct _ListCell ListCell;

#define LW_MAX_TEXT_LENGTH 127

struct _ListCell {
	char      text[LW_MAX_TEXT_LENGTH+1];
	ListCell *next_column;
};

typedef struct ListWidget {
	Window    *win;
	int        cols; /* Number of columns */
	int       *col_width; /* Ptr to array of int with each column's width */
	int        cursor_pos; /* Currently selected row */
	ListCell **rows_ref;
	int        rows; /* Row counter */
	int        first_visible_row;
	int        pos_marker;
} ListWidget;

ListWidget *listwidget_new(int cols, char *title, int x, int y, int width, int height);
int         listwidget_set_col_width(ListWidget *lw, int col, int width); /* width = -1 means column will take all available space */
int         listwidget_get_selection(ListWidget *lw); /* Returns the selected row number */
int         listwidget_get_rows(ListWidget *lw); /* Returns number of rows in list */
char       *listwidget_get_row_data(ListWidget *lw, int row, int col);
int         listwidget_delete_row(ListWidget *lw, int row);
int         listwidget_add_row(ListWidget *lw); /* Adds an empty row at the end of the list */
int         listwidget_insert_row(ListWidget *lw, int after_row); /* Inserts empty row in list */
int         listwidget_set_cell_data(ListWidget *lw, int row, int col, char *str);
int         listwidget_clear_all_rows(ListWidget *lw); /* Removes all rows from list */
int         listwidget_draw(ListWidget *lw);
void        listwidget_refresh(ListWidget *lw);
int         listwidget_set_cursor(ListWidget *lw, int pos);
int         listwidget_move_cursor(ListWidget *lw, int offset);
void        listwidget_resize(ListWidget *lw, int height, int width);
int         listwidget_set_length(ListWidget *lw, int rows); /* Sets the number of rows (adds or removes rows as neccessary) */
int         listwidget_free(ListWidget *lw);
#endif

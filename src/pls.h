/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: pls.h  Created: 110420
 *
 * Description: wejp's pls parser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _PLS_H
#define _PLS_H
#include <stdio.h>
#include "consts.h"

#define MAX_LINE_LENGTH 256

typedef struct PLS
{
	FILE  *pl_file;
	char   pls_path[PATH_LEN_DIR_MAX];
	short  version;
	char   current_item_title[MAX_LINE_LENGTH];
	char   current_item_filename[PATH_LEN_FILENAME_MAX];
	char   current_item_path[PATH_LEN_MAX];
	size_t current_item_length;
} PLS;

int    pls_open_file(PLS *pls, const char *filename);
void   pls_close_file(PLS *pls);
int    pls_read_next_item(PLS *pls);
char  *pls_current_item_get_title(PLS *pls);
char  *pls_current_item_get_filename(PLS *pls);
char  *pls_current_item_get_full_path(PLS *pls);
size_t pls_current_item_get_length(PLS *pls);
#endif

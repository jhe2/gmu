/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: m3u.h  Created: 061018
 *
 * Description: Wejp's m3u parser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _M3U_H
#define _M3U_H
#include <stdio.h>

#define MAX_PATH 256

typedef struct M3u
{
	FILE *pl_file;
	char  m3u_path[MAX_PATH];
	short extended;
	char  current_item_title[256];
	char  current_item_filename[MAX_PATH];
	char  current_item_path[MAX_PATH];
	int   current_item_length;
} M3u;

int   m3u_open_file(M3u *m3u, char *filename);
void  m3u_close_file(M3u *m3u);
int   m3u_read_next_item(M3u *m3u);
char *m3u_current_item_get_title(M3u *m3u);
char *m3u_current_item_get_filename(M3u *m3u);
char *m3u_current_item_get_full_path(M3u *m3u);
int   m3u_current_item_get_length(M3u *m3u);
int   m3u_is_extended(M3u *m3u);
int   m3u_export_file(M3u *m3u, char *filename);
int   m3u_export_write_entry(M3u *m3u, char *file, char *title, int length);
void  m3u_export_close_file(M3u *m3u);
#endif

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: m3u.c  Created: 061018
 *
 * Description: Wejp's m3u parser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "m3u.h"
#include "debug.h"
#include "charset.h"

int m3u_open_file(M3u *m3u, char *filename)
{
	int  result = 0, i, j;

	m3u->current_item_title[0]    = '\0';
	m3u->current_item_filename[0] = '\0';
	m3u->current_item_path[0]     = '\0';
	m3u->current_item_length      = 0;
	m3u->extended                 = 0;

	for (i = strlen(filename) - 1; i >= 0 && filename[i] != '/'; i--);
	for (j = 0; j <= i && j < MAX_PATH - 1; j++)
		m3u->m3u_path[j] = filename[j];
	m3u->m3u_path[j] = '\0';

	if (strlen(m3u->m3u_path) == 0) {
		int len;
		if (getcwd(m3u->m3u_path, MAX_PATH - 3)) {
			len = strlen(m3u->m3u_path);
			m3u->m3u_path[len] = '/';
			m3u->m3u_path[len+1] = '\0';
		} else {
			wdprintf(V_WARNING, "m3u", "WARNING: Unable to get current directory.\n");
			m3u->m3u_path[0] = '.';
			m3u->m3u_path[1] = '\0';
		}
	}
	wdprintf(V_DEBUG, "m3u", "Path = %s\n", m3u->m3u_path); 

	if ((m3u->pl_file = fopen(filename, "r")) != NULL) {
		char buf[MAX_PATH] = "";
		if (fgets(buf, MAX_PATH - 1, m3u->pl_file)) {
			if (strncmp(buf, "#EXTM3U", 7) == 0) {
				m3u->extended = 1;
				wdprintf(V_INFO, "m3u", "Extended playlist found.\n");
			} else {
				m3u->extended = 0;
				rewind(m3u->pl_file);
				wdprintf(V_INFO, "m3u", "Simple playlist found.\n");
			}
		} else {
			m3u->extended = 0;
			wdprintf(V_ERROR, "m3u", "Invalid playlist file. Empty file?\n");
		}
		result = 1;
	}
	
	return result;
}

void m3u_close_file(M3u *m3u)
{
	fclose(m3u->pl_file);
}

int m3u_read_next_item(M3u *m3u)
{
	int result = 0;
	if (m3u->extended) { /* Extended M3U */
		char buf[256] = " ";
		int  entry_found = 0;

		while (!feof(m3u->pl_file) && fgets(buf, 255, m3u->pl_file) != NULL && 
		       !(entry_found = (strncmp(buf, "#EXTINF", 7) == 0)));
		
		if (entry_found) {
			char mini_buffer[64] = "";
			int  i;
			for (i = 8; i < 255 && buf[i] != ','; i++);
			if (i < 255) { /* ok, we found a , that delimits length and title */
				char *rn = NULL;
				char  tmp_filename[256];
				strncpy(mini_buffer, buf+8, i-8);
				m3u->current_item_length = atoi(mini_buffer);
				strncpy(tmp_filename, buf+i+1, 255-i);
				if (charset_is_valid_utf8_string(tmp_filename)) {
					strncpy(m3u->current_item_title, tmp_filename, 255);
				} else {
					wdprintf(V_DEBUG, "m3u", "Invalid UTF-8 string found! Trying to interpret as ISO-8859-1...\n");
					int r = charset_iso8859_1_to_utf8(m3u->current_item_title, tmp_filename, 255);
					wdprintf(V_DEBUG, "m3u", "RES = %d\n", r);
				}

				if ((rn = strrchr(m3u->current_item_title, '\n')) != NULL)
					*rn = '\0';
				if ((rn = strrchr(m3u->current_item_title, '\r')) != NULL)
					*rn = '\0';
				if (fgets(m3u->current_item_filename, MAX_PATH - 1, m3u->pl_file)) {
					if ((rn = strrchr(m3u->current_item_filename, '\n')) != NULL)
						*rn = '\0';
					if ((rn = strrchr(m3u->current_item_filename, '\r')) != NULL)
						*rn = '\0';
					result = 1;
				}
			}
		}
	} else { /* Simple M3U */
		m3u->current_item_title[0] = '\0';
		m3u->current_item_length   = 0;
		if (fgets(m3u->current_item_filename, MAX_PATH - 1, m3u->pl_file) != NULL) {
			char *rn = NULL;
			if ((rn = strrchr(m3u->current_item_filename, '\n')) != NULL)
				*rn = '\0';
			if ((rn = strrchr(m3u->current_item_filename, '\r')) != NULL)
				*rn = '\0';
			if (strlen(m3u->current_item_filename) > 0)
				result = 1;
			if (charset_is_valid_utf8_string(m3u->current_item_filename))
				strncpy(m3u->current_item_title, m3u->current_item_filename, 255);
			else
				charset_iso8859_1_to_utf8(m3u->current_item_title, m3u->current_item_filename, 255);
		}
	}

	if (m3u->current_item_filename[0] != '/' && strncasecmp(m3u->current_item_filename, "http://", 7) != 0) {
		snprintf(m3u->current_item_path, MAX_PATH - 1, "%s%s", 
		         m3u->m3u_path, m3u->current_item_filename);
	} else {
		strncpy(m3u->current_item_path, m3u->current_item_filename, MAX_PATH - 1);
	}
	return result;
}

char *m3u_current_item_get_title(M3u *m3u)
{
	return m3u->current_item_title;
}

char *m3u_current_item_get_full_path(M3u *m3u)
{
	return m3u->current_item_path;
}

char *m3u_current_item_get_filename(M3u *m3u)
{
	return m3u->current_item_filename;
}

int m3u_current_item_get_length(M3u *m3u)
{
	return m3u->current_item_length;
}

int m3u_is_extended(M3u *m3u)
{
	return m3u->extended;
}

/* M3U Export */

int m3u_export_file(M3u *m3u, char *filename)
{
	int result = 0;
	if ((m3u->pl_file = fopen(filename, "w")) != NULL) {
		if (fputs("#EXTM3U\n", m3u->pl_file) != EOF)
			result = 1;
	}
	return result;
}

int m3u_export_write_entry(M3u *m3u, char *file, char *title, int length)
{
	int result = 0;
	if (m3u->pl_file && title != NULL && file != NULL)
		result = fprintf(m3u->pl_file, "#EXTINF:%d,%s\n%s\n", length, title, file);
	return result;
}

void m3u_export_close_file(M3u *m3u)
{
	fflush(m3u->pl_file);
	fclose(m3u->pl_file);
}

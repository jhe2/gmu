/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: pls.c  Created: 110420
 *
 * Description: wejp's pls parser
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
#include "pls.h"
#include "debug.h"

int pls_open_file(PLS *pls, char *filename)
{
	int  result = 0, i, j;

	pls->current_item_title[0]    = '\0';
	pls->current_item_filename[0] = '\0';
	pls->current_item_path[0]     = '\0';
	pls->current_item_length      = 0;

	for (i = strlen(filename) - 1; i >= 0 && filename[i] != '/'; i--);
	for (j = 0; j <= i && j < MAX_PATH - 1; j++)
		pls->pls_path[j] = filename[j];
	pls->pls_path[j] = '\0';

	if (strlen(pls->pls_path) == 0) {
		int len;
		if (getcwd(pls->pls_path, MAX_PATH - 3)) {
			len = strlen(pls->pls_path);
			pls->pls_path[len] = '/';
			pls->pls_path[len+1] = '\0';
		} else {
			wdprintf(V_WARNING, "pls", "WARNING: Unable to get current directory.\n");
			pls->pls_path[0] = '.';
			pls->pls_path[1] = '\0';
		}
	}
	wdprintf(V_DEBUG, "pls", "Path = %s\n", pls->pls_path); 

	if ((pls->pl_file = fopen(filename, "r")) != NULL) {
		char buf[MAX_PATH] = "";
		if (fgets(buf, MAX_PATH - 1, pls->pl_file)) {
			if (strncmp(buf, "[playlist]", 10) == 0) {
				wdprintf(V_INFO, "pls", "Looks like a PLS playlist file. Good.\n");
				result = 1;
			} else {
				fclose(pls->pl_file);
				wdprintf(V_INFO, "pls", "This is not a valid PLS playlist file.\n");
			}
		} else {
			wdprintf(V_ERROR, "pls", "Invalid playlist file. Empty file?\n");
		}
	}
	
	return result;
}

void pls_close_file(PLS *pls)
{
	fclose(pls->pl_file);
}

static int read_key_value_pair(PLS *pls, char *key, int key_size, 
                               char *value, int value_size)
{
	if (pls->pl_file && !feof(pls->pl_file)) {
		int  bufcnt;
		char ch = fgetc(pls->pl_file);
		/* Skip blanks... */
		if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
			while (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ch = fgetc(pls->pl_file);
		/* ...and comments (#)... */
		do {
			if (ch == '#' || ch == ';') {
				while (ch != '\n' && ch != '\r' && !feof(pls->pl_file)) ch = fgetc(pls->pl_file);
				ch = fgetc(pls->pl_file);
			}
			if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
				ch = fgetc(pls->pl_file);
		} while (ch == '#' || ch == ';' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

		bufcnt = 0;
		/* Read key name: */
		while (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && 
			   ch != '=' && !feof(pls->pl_file) && bufcnt < key_size-2) {
			key[bufcnt] = ch;
			bufcnt++;
			ch = fgetc(pls->pl_file);
		}
		key[bufcnt] = '\0';

		while (ch != '=' && !feof(pls->pl_file)) {
			ch = fgetc(pls->pl_file);
		}
		ch = fgetc(pls->pl_file);

		/* Skip blanks... */
		if (ch == ' ' || ch == '\t')
			while (ch == ' ' || ch == '\t') ch = fgetc(pls->pl_file);

		/* Read key value: */
		bufcnt = 0;
		while (ch != '\n' && ch != '\r' && !feof(pls->pl_file) && 
			   !feof(pls->pl_file) && bufcnt < value_size-2) {
			value[bufcnt] = ch;
			bufcnt++;
			ch = fgetc(pls->pl_file);
		}
		value[bufcnt] = '\0';
	}
	return !feof(pls->pl_file);
}

typedef enum PLSState {
	PLS_STATE_NONE = 0, PLS_STATE_FILE = 1, PLS_STATE_TITLE = 2, PLS_STATE_LENGTH = 4
} PLSSTate;

int pls_read_next_item(PLS *pls)
{
	char     key_buffer[MAX_LINE_LENGTH] = "", value_buffer[MAX_LINE_LENGTH] = "";
	PLSSTate state = PLS_STATE_NONE;
	long     pos;

	if (pls->pl_file) {
		int read_ok = 1;

		pls->current_item_length = -1;
		pls->current_item_title[0] = '\0';
		pls->current_item_filename[0] = '\0';
		while (!(state == PLS_STATE_FILE + PLS_STATE_TITLE + PLS_STATE_LENGTH) && read_ok) {
			int size;
			pos = ftell(pls->pl_file);
			read_ok = read_key_value_pair(pls, key_buffer, MAX_LINE_LENGTH, value_buffer, MAX_LINE_LENGTH);
			if (strncasecmp(key_buffer, "File", 4) == 0) { /* Playlist entry found */
				if (!(state & PLS_STATE_FILE)) {
					size = strlen(value_buffer);
					size = size > MAX_LINE_LENGTH-1 ? MAX_LINE_LENGTH-1 : size;
					memcpy(pls->current_item_filename, value_buffer, size);
					pls->current_item_filename[size] = '\0';
					state |= PLS_STATE_FILE;
				} else {
					fseek(pls->pl_file, pos, SEEK_SET);
					break;
				}
			}
			if (strncasecmp(key_buffer, "Title", 5) == 0) {
				if (!(state & PLS_STATE_TITLE)) {
					size = strlen(value_buffer);
					size = size > MAX_LINE_LENGTH-1 ? MAX_LINE_LENGTH-1 : size;
					memcpy(pls->current_item_title, value_buffer, size);
					pls->current_item_title[size] = '\0';
					state |= PLS_STATE_TITLE;
				} else {
					fseek(pls->pl_file, pos, SEEK_SET);
					break;
				}
			}
			if (strncasecmp(key_buffer, "Length", 5) == 0) {
				if (!(state & PLS_STATE_LENGTH)) {
					pls->current_item_length = atoi(value_buffer);
					state |= PLS_STATE_LENGTH;
				} else {
					fseek(pls->pl_file, pos, SEEK_SET);
					break;
				}
			}
		}
	}

	if (pls->current_item_filename[0] != '/' && strncasecmp(pls->current_item_filename, "http://", 7) != 0) {
		snprintf(pls->current_item_path, MAX_PATH - 1, "%s%s", 
		         pls->pls_path, pls->current_item_filename);
	} else {
		strncpy(pls->current_item_path, pls->current_item_filename, MAX_PATH - 1);
	}
	return (state & PLS_STATE_FILE) ? 1 : 0;
}

char *pls_current_item_get_title(PLS *pls)
{
	return pls->current_item_title;
}

char *pls_current_item_get_full_path(PLS *pls)
{
	return pls->current_item_path;
}

char *pls_current_item_get_filename(PLS *pls)
{
	return pls->current_item_filename;
}

int pls_current_item_get_length(PLS *pls)
{
	return pls->current_item_length;
}

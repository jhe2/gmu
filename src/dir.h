/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: dir.h  Created: 060929
 *
 * Description: Directory parser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#define MAX_FILES 16384
#define REG_FILE  2
#define DIRECTORY 3
#ifndef _DIR_H
#define _DIR_H
struct _Dir
{
	struct dirent **ep;
	/* "entries" contains all fetched files, while "files" contains the number of visible files */
	int             entries, files;
	short           flag_tmp[MAX_FILES];
	short           flag[MAX_FILES];
	long            filesize_tmp[MAX_FILES];
	long            filesize[MAX_FILES];
	char           *filename[MAX_FILES];
	char            path[256];
	char            base_dir[256];
};

typedef struct _Dir Dir;

void  dir_init(Dir *dir);
void  dir_set_base_dir(Dir *dir, char *base_dir);
char *dir_get_base_dir(Dir *dir);
void  dir_set_ext_filter(const char **dir_exts, int show_dirs);
int   dir_read(Dir *dir, char *path, int directories_first);
void  dir_free(Dir *dir);
char *dir_get_filename(Dir *dir, int i);
char *dir_get_filename_with_full_path_alloc(Dir *dir, int i);
long  dir_get_filesize(Dir *dir, int i);
void  dir_get_human_readable_filesize(Dir *dir, int i,
                                      char *target, int target_size);
int   dir_get_number_of_files(Dir *dir);
int   dir_get_flag(Dir *dir, int i);
char *dir_get_path(Dir *dir);
char *dir_get_new_dir_alloc(char *current_dir, char *new_dir);
#endif

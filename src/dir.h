/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
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
	char          **dir_extensions;
	int             show_directories;
};

typedef struct _Dir Dir;

Dir  *dir_init(void);
void  dir_set_base_dir(Dir *dir, const char *base_dir);
char *dir_get_base_dir(Dir *dir);
/* dir_set_ext_filter() expects a pointer to a _statically_ allocated array of file extensions! */
void  dir_set_ext_filter(Dir *dir, char **dir_exts, int show_dirs);
int   dir_read(Dir *dir, const char *path, int directories_first);
void  dir_clear(Dir *dir);
void  dir_free(Dir *dir);
char *dir_get_filename(Dir *dir, int i);
char *dir_get_filename_with_full_path_alloc(Dir *dir, int i);
long  dir_get_filesize(Dir *dir, int i);
void  dir_get_human_readable_filesize(Dir *dir, int i,
                                      char *target, int target_size);
int   dir_get_number_of_files(Dir *dir);
int   dir_get_flag(Dir *dir, int i);
char *dir_get_path(Dir *dir);
char *dir_get_new_dir_alloc(const char *current_dir, const char *new_dir);
#endif

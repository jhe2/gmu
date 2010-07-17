/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: dir.c  Created: 060929
 *
 * Description: Directory parser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "dir.h"
#include "util.h"

static const char **dir_extensions = NULL;
static int          show_directories;

void dir_set_ext_filter(const char **dir_exts, int show_dirs)
{
	dir_extensions = dir_exts;
	show_directories = show_dirs;
}

static int select_file(const struct dirent *de)
{
	int         result = 0, i, filename_len, ext_len;
	struct stat attr;
	char        extension[16];

	if (dir_extensions) {
		for (i = 0; dir_extensions[i] != NULL; i++) {
			if (stat(de->d_name, &attr) != -1) {
				if (S_ISDIR(attr.st_mode) && show_directories) {
					result = 1;
					break;
				}
			}
			filename_len = strlen(de->d_name);
			ext_len      = strlen(dir_extensions[i]);
			ext_len      = (ext_len > 15 ? 15 : ext_len);
			strtolower(extension, de->d_name+filename_len-ext_len, ext_len+1);
			if (strncmp(extension, dir_extensions[i], ext_len) == 0) {
				result = 1;
				break;
			}
		}
	} else {
		result = 1;
	}
	return result;
}

int dir_read(Dir *dir, char *path, int directories_first)
{
	int  i, j, result = 0;
	char prev_wd[256];

	if (getcwd(prev_wd, 255)) {
		if (chdir(path) == 0) {
			if (getcwd(dir->path, 255)) {
				dir->files = scandir(path, &(dir->ep), select_file, alphasort);
				if (dir->files > MAX_FILES) dir->files = MAX_FILES;
				for (i = 0; i < dir->files; i++) {
					struct stat attr;
					if (lstat(dir->ep[i]->d_name, &attr) != -1) {
						if (S_ISREG(attr.st_mode))
							dir->flag_tmp[i] = REG_FILE;
						else if (S_ISDIR(attr.st_mode) && !S_ISLNK(attr.st_mode))
							dir->flag_tmp[i] = DIRECTORY;
						else
							dir->flag_tmp[i] = -2;
						dir->filesize_tmp[i] = attr.st_size;
					} else {
						dir->flag_tmp[i] = -1;
						dir->filesize_tmp[i] = -1;
					}
				}
				/* directories first: */
				if (directories_first) {
					for (i = 0, j = 0; i < dir->files; i++) {
						if (dir->flag_tmp[i] == DIRECTORY) {
							dir->filename[j] = dir->ep[i]->d_name;
							dir->flag[j]     = dir->flag_tmp[i];
							dir->filesize[j] = dir->filesize_tmp[i];
							j++;
						}
					}
					for (i = 0; i < dir->files; i++) {
						if (dir->flag_tmp[i] != DIRECTORY) {
							dir->filename[j] = dir->ep[i]->d_name;
							dir->flag[j]     = dir->flag_tmp[i];
							dir->filesize[j] = dir->filesize_tmp[i];
							j++;
						}
					}
				} else {
					for (i = 0; i < dir->files; i++) {
						dir->filename[i] = dir->ep[i]->d_name;
						dir->flag[i]     = dir->flag_tmp[i];
						dir->filesize[i] = dir->filesize_tmp[i];
					}
				}
				result = 1;
			}
		}
		if (chdir(prev_wd))
			printf("dir: ERROR: Unable to change directory to parent dir.\n");
	}
	return result;
}

void dir_free(Dir *dir)
{
	int             i;
	struct dirent **list;

	for (i = 0, list = dir->ep; i < dir->files; i++, list++)
		free(*list);
	free(dir->ep);
}

char *dir_get_filename(Dir *dir, int i)
{
	/*return (i < (dir->files) ? dir->ep[i]->d_name : NULL);*/
	return (i < (dir->files) ? dir->filename[i] : NULL);
}

long dir_get_filesize(Dir *dir, int i)
{
	return (i < (dir->files) ? dir->filesize[i] : -1);
}

void dir_get_human_readable_filesize(Dir *dir, int i, 
                                     char *target, int target_size)
{
	long       s = dir_get_filesize(dir, i);
	int        result = -1;
	char       suffix = 'T';
	const long K = 1000, M = K * 1000, G = M * 1000;

	if (s < 0) {
		suffix = '?';
		result = 0;
	} else if (s < K) {
		suffix = 'B';
		result = s;
	} else if (s < M) {
		suffix = 'K';
		result = s / K;
	} else if (s < G) {
		suffix = 'M';
		result = s / M;
	} else {
		suffix = 'G';
		result = s / G;
		if (s > 999) s = 999;
	}
	if (suffix == '?')
		snprintf(target, target_size, "?");
	else
		snprintf(target, target_size, "%3d%c", result, suffix);
}

int dir_get_flag(Dir *dir, int i)
{
	return (i < dir->files ? dir->flag[i] : -1);
}

int dir_get_number_of_files(Dir *dir)
{
	return dir->files;
}

char *dir_get_path(Dir *dir)
{
	return dir->path;
}

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
#include "debug.h"
#include "charset.h"

static const char **dir_extensions = NULL;
static int          show_directories;

void dir_set_ext_filter(const char **dir_exts, int show_dirs)
{
	dir_extensions = dir_exts;
	show_directories = show_dirs;
}

static int select_file(const struct dirent *de)
{
	/* I would like to use this function to filter the results, 
	 * but scandir()'s design is so broken, that it is impossible 
	 * (can't supply any additional arguments to this function) */
	return 1;
}

void dir_init(Dir *dir)
{
	dir->entries = 0;
	dir->files = 0;
	dir->ep = NULL;
	dir->path[0] = '\0';
	dir->base_dir[0] = '\0';
}

void dir_set_base_dir(Dir *dir, char *base_dir)
{
	if (base_dir) {
		if (strlen(base_dir) < 256)
			strncpy(dir->base_dir, base_dir, 255);
		else
			dir->base_dir[0] = '\0';
	}
}

char *dir_get_base_dir(Dir *dir)
{
	return dir->base_dir;
}

int dir_read(Dir *dir, char *path, int directories_first)
{
	int   i, j, result = 0;
	char *new_path = NULL;

	/* Treat an empty current path (or a single dot) as current work directory */
	if (dir->path[0] == '\0' || (dir->path[0] == '.' && dir->path[1] == '\0'))
		if (!getcwd(dir->path, 255)) dir->path[0] = '\0';
	
	if (dir->path[0] && dir->base_dir[0]) {
		new_path = dir_get_new_dir_alloc(dir->path, path);
		wdprintf(V_DEBUG, "dir", "old path=%s\nnew path=%s\n", dir->path, new_path);
		if (new_path && strncmp(new_path, dir->base_dir, strlen(dir->base_dir)) == 0) {
			memset(dir->path, 0, 256);
			strncpy(dir->path, new_path, 255);
			wdprintf(V_DEBUG, "dir", "scanning path=[%s]\n", dir->path);
			dir_free(dir);
			dir->entries = scandir(dir->path, &(dir->ep), select_file, alphasort);
			dir->files = dir->entries;
			wdprintf(V_DEBUG, "dir", "files found=%d\n", dir->files);
			if (dir->files > MAX_FILES) dir->files = MAX_FILES;
			for (i = 0; i < dir->files; i++) {
				struct stat attr;
				char        tmp[256];
				
				snprintf(tmp, 255, "%s/%s", dir->path, dir->ep[i]->d_name);
				if (lstat(tmp, &attr) != -1) {
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
				int files_count = dir->files;
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
						int k;
						if (dir_extensions) {
							int res = 0;
							for (k = 0; dir_extensions[k] != NULL; k++) {
								int filename_len = strlen(dir->ep[i]->d_name);
								int ext_len      = strlen(dir_extensions[k]);
								ext_len      = (ext_len > 15 ? 15 : ext_len);
								if (filename_len-ext_len >= 0) {
									if (strncasecmp(dir->ep[i]->d_name+filename_len-ext_len, dir_extensions[k], ext_len) == 0) {
										dir->filename[j] = dir->ep[i]->d_name;
										dir->flag[j]     = dir->flag_tmp[i];
										dir->filesize[j] = dir->filesize_tmp[i];
										j++;
										res = 1;
										break;
									}
								}
							}
							if (!res) files_count--;
						}
					}
				}
				dir->files = files_count;
			} else {
				for (i = 0; i < dir->files; i++) {
					dir->filename[i] = dir->ep[i]->d_name;
					dir->flag[i]     = dir->flag_tmp[i];
					dir->filesize[i] = dir->filesize_tmp[i];
				}
			}
			if (dir->files > 0) result = 1;
		}
		if (new_path) free(new_path);
	}
	return result;
}

void dir_free(Dir *dir)
{
	int             i;
	struct dirent **list;

	if (dir->ep) {
		for (i = 0, list = dir->ep; i < dir->entries; i++, list++)
			free(*list);
		free(dir->ep);
		dir->ep = NULL;
	}
}

char *dir_get_filename(Dir *dir, int i)
{
	char *str = (i < (dir->files) ? dir->filename[i] : NULL);
	/*return (i < (dir->files) ? dir->ep[i]->d_name : NULL);*/
	if (str && !charset_is_valid_utf8_string(str))
		wdprintf(V_WARNING, "dir", "Invalid UTF-8 filename found: [%s]\n", str);
	return str;
}

char *dir_get_filename_with_full_path_alloc(Dir *dir, int i)
{
	char *fn = dir_get_filename(dir, i);
	char *res = NULL;
	if (fn) {
		int len = strlen(dir->path) + strlen(fn) + 3;
		res = malloc(len);
		if (res) {
			snprintf(res, len-1, "%s%s", dir->path, fn);
		}
	}
	return res;
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

/* For a given current directory and a new directory (which can be 
 * absolute or relative to the current directory), the function
 * returns a newly allocated string with the absolute path of the
 * new directory */
char *dir_get_new_dir_alloc(char *current_dir, char *new_dir)
{
	char *new_dir_full = NULL, *new_dir_temp;
	if (current_dir && new_dir) {
		int len = 0, len_new_dir, len_current_dir;
		
		len_new_dir = strlen(new_dir);
		len_current_dir = strlen(current_dir);
		if (new_dir[0] != '/') { /* no absolute path given */
			len = len_current_dir + len_new_dir + 1; /* maxmimum length possibly needed */
		} else { /* absolute path given */
			len = len_new_dir + 1;
		}
		wdprintf(V_DEBUG, "dir", "len total=%d len cur=%d len new=%d\n", len, len_current_dir, len_new_dir);
		if (len > 0) {
			new_dir_full = malloc(len+2);
			new_dir_temp = malloc(len+2);
			if (new_dir_full && new_dir_temp) {
				int s, t;
				if (new_dir[0] != '/') { /* no absolute path given */
					memcpy(new_dir_temp, current_dir, len_current_dir);
					if (current_dir[len_current_dir-1] != '/') {
						new_dir_temp[len_current_dir] = '/';
						len_current_dir++;
						len++;
					}
					memcpy(new_dir_temp+len_current_dir, new_dir, len_new_dir+1);
				} else { /* absolute path given */
					memcpy(new_dir_temp, new_dir, len_new_dir+1);
				}
				/* At this point new_dir_temp contains an absolute, but 
				 * possibly unsafe and unneccessarily long path (could 
				 * contain "..") */
				wdprintf(V_DEBUG, "dir", "path='%s'\n", new_dir_temp);
				/* Convert path such that it has the shortest possible
				 * form of an absolute path */
				for (s = 0, t = 0; s < len; s++) {
					if (new_dir_temp[s]    == '/' && s < len-3 && 
					    new_dir_temp[s+1]  == '.' && new_dir_temp[s+2] == '.' &&
					    (new_dir_temp[s+3] == '/' || new_dir_temp[s+3] == '\0')) {
						s += 2;
						/* Remove last directory from full path */
						while (t > 1 && new_dir_full[t-1] != '/') t--;
						if (t > 0) t--;
						continue;
					} else if (new_dir_temp[s] == '/' && s < len-2 && 
					    new_dir_temp[s+1]  == '.' &&
					    (new_dir_temp[s+2] == '/' || new_dir_temp[s+2] == '\0')) {
						s += 1;
						continue;
					}
					/* Concatenate character */
					new_dir_full[t] = new_dir_temp[s];
					new_dir_full[t+1] = '\0';
					t++;
				}

				/* Attach / at the end, if missing */
				{
					int size = strlen(new_dir_full);
					if (size == 0 || new_dir_full[size-1] != '/') {
						new_dir_full[size] = '/';
						new_dir_full[size+1] = '\0';
					}
				}
			}
			free(new_dir_temp);
		}
	}
	return new_dir_full;
}

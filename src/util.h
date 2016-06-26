/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
 *
 * File: util.h  Created: 060929
 *
 * Description: Misc. utilitiy functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _UTIL_H
#define _UTIL_H
#include <stdio.h>
#include "gmudecoder.h"

void  strtoupper(char *target, const char *src, size_t len);
void  strtolower(char *target, const char *src, size_t len);
int   file_exists(const char *filename);
int   file_copy(const char *destination_file, const char *source_file);
const char *get_file_extension(const char *filename);
const char *extract_filename_from_path(const char *path);
int   get_first_matching_file(
	char       *target,
	size_t      target_length,
	const char *path,
	const char *pattern
);
int   get_first_matching_file_pattern_list(
	char       *target,
	size_t      target_length, 
	const char *path,
	const char *pattern_list
);
char *get_file_matching_given_pattern_alloc(
	const char *original_file,
	const char *file_pattern
);
int   strncpy_charset_conv(
	char       *target,
	const char *source,
	size_t      target_size,
	size_t      source_size,
	GmuCharset  charset
);
/**
 * expand_path_alloc() takes a file system path string as input and if it
 * begins with a '~', it expands that to the user's home directory to
 * form a complete absolute path. Any other type of input path is passed
 * through, left unchanged. In case of error, the function might return NULL.
 */
char *expand_path_alloc(const char *path);
int assign_signal_handler(int sig_num, void (*signalhandler)(int));

/**
 * Creates a directory path with all sub directories as necessary.
 * Returns 0 on success or -1 on error (and set errno).
 */
int rmkdir(const char *dir, mode_t mode);

/**
 * Returns the user's home directory.
 */
const char *get_home_dir(void);

/**
 * Returns the user's data directory.
 * If 'create' is true, the function tries to create the directory,
 * if it does not exist. Returns NULL on failure. The returned value
 * needs to be free'd when it is no longer used.
 */
char *get_data_dir_alloc(int create);

/**
 * Returns the user's config file directory.
 * If 'create' is true, the function tries to create the directory,
 * if it does not exist. Returns NULL on failure. The returned value
 * needs to be free'd when it is no longer used.
 */
char *get_config_dir_alloc(int create);

/**
 * Returns the config file directory for a given application name.
 * If 'create' is true, the function tries to create the directory,
 * if it does not exist. If filename is not NULL, the path includes
 * that filename. If 'create' is true and a filename is given, the
 * path up to that file is created if necessary, but the file is not.
 * Returns NULL on failure. The returned value needs to be free'd when
 * it is no longer used.
 */
char *get_config_dir_with_name_alloc(const char *name, int create, const char *filename);

/**
 * Returns the data directory for a given application name.
 * If 'create' is true, the function tries to create the directory,
 * if it does not exist. If filename is not NULL, the path includes
 * that filename. If 'create' is true and a filename is given, the
 * path up to that file is created if necessary, but the file is not.
 * Returns NULL on failure. The returned value needs to be free'd when
 * it is no longer used.
 */
char *get_data_dir_with_name_alloc(const char *name, int create, const char *filename);

/**
 * Returns a valid path to the requested config file (if available),
 * or NULL if the file is unaccessible. The path might be relative to
 * the current working directory. Example return values:
 * get_config_file_path_alloc("foo", "bar.conf")
 * -> "/home/user/.config/foo/bar.conf" or "bar.conf"
 */
char *get_config_file_path_alloc(const char *program_name, const char *filename);
#endif

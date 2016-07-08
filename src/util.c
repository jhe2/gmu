/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
 *
 * File: util.c  Created: 060929
 *
 * Description: Misc. utilitiy functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h> /* for mkdir() */
#include <errno.h>
#include <limits.h> /* for PATH_MAX */
#include "charset.h"
#include "debug.h"
#include "util.h"

void strtoupper(char *target, const char *src, size_t len)
{
	size_t i, srclen = strlen(src);

	for (i = 0; i < (srclen < len - 1 ? srclen : len - 1); i++)
		target[i] = toupper(src[i]);
	target[i] = '\0';
}

void strtolower(char *target, const char *src, size_t len)
{
	size_t i, srclen = strlen(src);

	for (i = 0; i < (srclen < len - 1 ? srclen : len - 1); i++)
		target[i] = tolower(src[i]);
	target[i] = '\0';
}

int file_exists(const char *filename)
{
	int   result = 0;
	FILE *file = fopen(filename, "r");

	if (file) {
		result = 1;
		fclose(file);
	}
	return result;
}

int file_copy(const char *destination_file, const char *source_file)
{
	int   result = 1;
	FILE *inf, *ouf;

	wdprintf(V_INFO, "util", "Copying %s to %s.\n", source_file, destination_file);
	inf = fopen(source_file, "rb");
	if (inf) {
		ouf = fopen(destination_file, "wb");
		if (ouf) {
			while (!feof(inf)) {
				char ch = fgetc(inf);
				if (!feof(inf)) {
					if (fputc(ch, ouf) == EOF) {
						result = 0;
						break;
					}
				}
			}
			fclose(ouf);
		} else {
			result = 0;
		}
		fclose(inf);
	} else {
		result = 0;
	}
	return result;
}

const char *get_file_extension(const char *filename)
{
	size_t len = filename ? strlen(filename) : 0;
	while (len > 0 && filename[len] != '.') len--;
	return (len == 0 ? NULL : filename + len + 1);
}

const char *extract_filename_from_path(const char *path)
{
	const char *filename_without_path = strrchr(path, '/');
	if (filename_without_path != NULL)
		filename_without_path++;
	else
		filename_without_path = path;
	return filename_without_path;
}

static unsigned match_pattern(const char *string, const char *pattern)
{
	unsigned result = 1;
	size_t   i, j = 0;

	for (i = 0; i < strlen(pattern); i++) {
		if (pattern[i] == '*' && i+1 < strlen(pattern)) {
			while (pattern[i+1] != string[j] && j < strlen(string))
				j++;
			if (pattern[i+1] != string[j]) result = 0;
		} else if (pattern[i] == '?') {
			j++;
		} else {
			if (pattern[i] != string[j]) /* what the bug?? */
				result = 0;
			else
				j++;
		}
	}
	return result;
}

int get_first_matching_file(
	char       *target,
	size_t      target_length,
	const char *path,
	const char *pattern
)
{
	struct dirent **ep = NULL;
	int             result = 0, entries = 0;

	if ((entries = scandir(path, &ep, 0, alphasort)) > 0) {
		int i;
		for (i = 0; i < entries; i++) {
			if (match_pattern(ep[i]->d_name, pattern)) {
				strncpy(target, ep[i]->d_name, target_length);
				result = 1;
				break;
			}
		}
		for (i = 0; i < entries; i++)
			free(ep[i]);
	}
	
	if (ep != NULL) free(ep);
	return result;
}

static int check_pattern(
	char       *target,
	size_t      target_length,
	const char *path,
	const char *pattern_list,
	size_t      pattern_offset,
	size_t      pattern_length
)
{
	int res = 0;
	if (pattern_length > 0) {
		char *pattern = calloc(1, pattern_length + 1);
		if (pattern) {
			int k;
			for (k = pattern_offset; k < pattern_length + pattern_offset; k++)
				pattern[k-pattern_offset] = pattern_list[k];
			pattern[k-pattern_offset] = '\0';
			/*wdprintf(V_DEBUG, "util", "Pattern: %s\n", pattern);*/
			res = get_first_matching_file(target, target_length, path, pattern);
			free(pattern);
		}
	}
	return res;
}

/* pattern_list is a string with a list of patterns seperated by semicolons ';' */
int get_first_matching_file_pattern_list(
	char       *target,
	size_t      target_length,
	const char *path,
	const char *pattern_list
)
{
	size_t i, j;
	int    res = 0;

	for (i = 0, j = 0; pattern_list[i] != '\0'; i++) { /* bug */
		if (pattern_list[i] == ';') {
			res = check_pattern(target, target_length, path,
			                    pattern_list, j, i-j);
			if (res) break;
			j = i + 1;
		}
	}
	if (!res && pattern_list[i] == '\0') {
		res = check_pattern(target, target_length, path,
		                    pattern_list, j, i-j);
	}
	return res;
}

#define MAX_REPLACE_STR_LENGTH 256
static char *replace_char_with_string_alloc(
	const char *str,
	const char char_to_replace,
	const char *str_to_insert
)
{
	size_t i, k, len = strlen(str);
	size_t len_str_to_insert = strlen(str_to_insert);
	char  *res_str = malloc(MAX_REPLACE_STR_LENGTH);

	if (res_str) {
		for (i = 0, k = 0; i < len && k < MAX_REPLACE_STR_LENGTH; i++)
			if (str[i] == char_to_replace) {
				int copy_len = len_str_to_insert > MAX_REPLACE_STR_LENGTH - k ?
												   MAX_REPLACE_STR_LENGTH - k : len_str_to_insert;
				strncpy(res_str+k, str_to_insert, copy_len);
				k += copy_len;
			} else {
				res_str[k] = str[i];
				k++;
			}
		res_str[k] = '\0';
	}
	return res_str;
}

char *get_file_matching_given_pattern_alloc(
	const char *original_file,
	const char *file_pattern
)
{
	size_t       path_length = 0, filename_without_ext_length = 0;
	char        *d = strrchr(original_file, '/');
	char        *ext = (d ? strrchr(d, '.') : strrchr(original_file, '.'));
	char         path[256] = "", filename_without_ext[256] = "";
	char         new_file[256] = "";
	char        *pattern = NULL;
	char        *res_str = NULL;

	if (d != NULL) {
		path_length = d - original_file;
		if (ext != NULL)
			filename_without_ext_length = ext - d - 1;
		strncpy(filename_without_ext, d+1, filename_without_ext_length);
	}
	strncpy(path, original_file, path_length);

	if (filename_without_ext[0] != '\0') {
		pattern = replace_char_with_string_alloc(file_pattern, '$', filename_without_ext);
	}

	if (get_first_matching_file_pattern_list(new_file, 256, path, pattern ? pattern : file_pattern)) {
		res_str = malloc(256);
		if (res_str) {
			snprintf(res_str, 255, "%s/%s", path, new_file);
			res_str[255] = '\0';
		}
	}
	if (filename_without_ext[0] != '\0') free(pattern);
	return res_str;
}

int strncpy_charset_conv(
	char       *target,
	const char *source,
	size_t      target_size,
	size_t      source_size,
	GmuCharset  charset
)
{
	int res = 0;
	switch (charset) {
		case M_CHARSET_ISO_8859_1:
		case M_CHARSET_ISO_8859_15:
			res = charset_iso8859_1_to_utf8(target, source, target_size);
			break;
		case M_CHARSET_UTF_8:
			if (charset_is_valid_utf8_string(source)) {
				strncpy(target, source, target_size);
				res = 1;
			} else {
				target[0] = '\0';
			}
			break;
		case M_CHARSET_UTF_16_BOM:
			res = charset_utf16_to_utf8(target, target_size, source, source_size, BOM);
			break;
		case M_CHARSET_UTF_16_BE:
			res = charset_utf16_to_utf8(target, target_size, source, source_size, BE);
			break;
		case M_CHARSET_UTF_16_LE:
			res = charset_utf16_to_utf8(target, target_size, source, source_size, LE);
			break;
		case M_CHARSET_AUTODETECT:
			wdprintf(V_DEBUG, "fileplayer", "Charset autodetect!\n");
			if (charset_is_valid_utf8_string(source)) {
				strncpy(target, source, target_size);
				res = 1;
			} else {
				if (!(res = charset_utf16_to_utf8(target, target_size, source, source_size, BOM)))
					res = charset_iso8859_1_to_utf8(target, source, target_size);
			}
			break;
	}
	return res;
}

char *expand_path_alloc(const char *path)
{
	char *res = NULL;
	char *home = getenv("HOME");
	if (home && path) {
		size_t len_p = strlen(path);
		size_t len_h = strlen(home);
		res = malloc(len_p + (path[0] == '~' ? len_h : 0) + 1);
		if (res) {
			snprintf(res, len_p + len_h + 1, "%s%s",
			         path[0] == '~' ? home : "",
			         path[0] == '~' ? path + 1 : path);
		}
	}
	return res;
}

int assign_signal_handler(int sig_num, void (*signalhandler)(int))
{
	int              res = 1;
	struct sigaction new_sig;

	new_sig.sa_handler = signalhandler;
	sigemptyset(&new_sig.sa_mask);
	new_sig.sa_flags = SA_RESTART;
	if (sigaction(sig_num, &new_sig, NULL) < 0) res = 0;
	return res;
}

int rmkdir(const char *dir, mode_t mode)
{
	int    res = -1;
	char   tmp[PATH_MAX];
	char  *p = NULL;
	size_t len, size = sizeof(tmp);
	errno = 0;

	len = snprintf(tmp, size, "%s", dir);
	if (len > 0 && len < size) {
		if (tmp[len - 1] == '/')
			tmp[len - 1] = 0;
		for (p = tmp + 1; *p; p++) {
			if (*p == '/') {
				*p = 0;
				mkdir(tmp, mode);
				*p = '/';
			}
		}
		res = mkdir(tmp, mode);
	}
	return res;
}

const char *get_home_dir(void)
{
	const char *home_dir;

	if ((home_dir = getenv("HOME")) == NULL) {
		home_dir = getpwuid(getuid())->pw_dir;
	}
	return home_dir;
}

static char *get_xdg_dir_alloc(const char *xdg_var_str, const char *alt_path, int create)
{
	char       *xdg_dir = NULL;
	const char *tmp;

	if ((tmp = getenv(xdg_var_str)) != NULL) {
		size_t len = strlen(tmp);
		if (len > 1) {
			xdg_dir = malloc(len + 1);
			if (xdg_dir) strncpy(xdg_dir, tmp, len + 1);
		}
	} else {
		/* Try with $HOME/alt_path instead (e.g ~/.config/gmu or ~/.local/share/gmu)... */
		const char *home = get_home_dir();
		if (home) {
			size_t len = strlen(home);
			if (len > 1) {
				size_t alt_path_len = alt_path ? strlen(alt_path) : 0;
				if (alt_path_len > 0) {
					xdg_dir = malloc(len + 1 + alt_path_len + 1);
					if (xdg_dir) snprintf(xdg_dir, len + 1 + alt_path_len + 1, "%s/%s", home, alt_path);
				}
			}
		}
	}
	if (create) rmkdir(xdg_dir, S_IRWXU);
	return xdg_dir;
}

char *get_data_dir_alloc(int create)
{
	return get_xdg_dir_alloc("XDG_DATA_HOME", ".local/share", create);
}

char *get_config_dir_alloc(int create)
{
	return get_xdg_dir_alloc("XDG_CONFIG_HOME", ".config", create);
}

static char *get_dir_alloc(char *dir, const char *name, int create, const char *filename)
{
	if (dir && name) {
		size_t len_name = strlen(name);
		size_t len_filename = filename ? strlen(filename) : 0;

		if (len_name > 1) {
			size_t len_config_dir = strlen(dir);
			if (len_config_dir > 1) {
				size_t len_total = len_config_dir + 1 + len_name + 1 + len_filename + 1;
				dir = realloc(dir, len_total);
				strcat(dir, "/");
				strcat(dir, name);
				if (create) rmkdir(dir, S_IRWXU);
				if (filename) {
					strcat(dir, "/");
					strcat(dir, filename);
				}
			}
		}
	}
	return dir;
}

char *get_config_dir_with_name_alloc(const char *name, int create, const char *filename)
{
	char *config_dir = get_config_dir_alloc(create);
	return get_dir_alloc(config_dir, name, create, filename);
}

char *get_data_dir_with_name_alloc(const char *name, int create, const char *filename)
{
	char *data_dir = get_data_dir_alloc(create);
	return get_dir_alloc(data_dir, name, create, filename);
}

char *get_config_file_path_alloc(const char *program_name, const char *filename)
{
	char  *file_path = NULL;
	size_t len = 0;
	/*
	 * Test in user-config-dir first.
	 * Then try system-config-dir.
	 * Last, try current working directory.
	 */
	file_path = get_config_dir_with_name_alloc(program_name, 0, filename);
	if (access(file_path, R_OK) != 0) { /* Access in user config dir failed */
		free(file_path);
		/* Try system install path... */
		len = strlen(GMU_INSTALL_PREFIX);
		if (len > 0) {
			size_t size = len + 5 + strlen(program_name) + 1 + strlen(filename) + 1;
			file_path = malloc(size);
			if (file_path)
				snprintf(file_path, size, "%s/etc/%s/%s", GMU_INSTALL_PREFIX, program_name, filename);
		}
		if (file_path && access(file_path, R_OK) != 0) {
			free(file_path);
			file_path = NULL;
			if (access(filename, R_OK) == 0) { /* Try access in current working dir */
				len = strlen(filename);
				if (len > 0) {
					file_path = malloc(len + 1);
					if (file_path) {
						strncpy(file_path, filename, len + 1);
					}
				}
			}
		}
	}
	return file_path;
}

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
#include "charset.h"
#include "debug.h"
#include "util.h"

void strtoupper(char *target, const char *src, int len)
{
	int i, srclen = strlen(src);

	for (i = 0; i < (srclen < len - 1 ? srclen : len - 1); i++)
		target[i] = toupper(src[i]);
	target[i] = '\0';
}

void strtolower(char *target, const char *src, int len)
{
	int i, srclen = strlen(src);

	for (i = 0; i < (srclen < len - 1 ? srclen : len - 1); i++)
		target[i] = tolower(src[i]);
	target[i] = '\0';
}

int file_exists(char *filename)
{
	int   result = 0;
	FILE *file = fopen(filename, "r");

	if (file) {
		result = 1;
		fclose(file);
	}
	return result;
}

int file_copy(char *destination_file, char *source_file)
{
	int result = 1;
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

char *get_file_extension(char *filename)
{
	int l = strlen(filename);
	while (filename[l] != '.' && l > 0) l--;
	return (l == 0 ? NULL : filename+l+1);
}

static int match_pattern(char *string, char *pattern)
{
	unsigned int result = 1, i, j = 0;
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

int get_first_matching_file(char *target, int target_length, char *path, char *pattern)
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

static int check_pattern(char *target,       int target_length,  char *path,
                         char *pattern_list, int pattern_offset, int   pattern_length)
{
	int res = 0;
	if (pattern_length > 0) {
		char *pattern = malloc(pattern_length + 1);
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
int get_first_matching_file_pattern_list(char *target, int   target_length, 
                                         char *path,   char *pattern_list)
{
	int i, j, res = 0;
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
static char *replace_char_with_string_alloc(char *str, char char_to_replace, char *str_to_insert)
{
	int   i, k, l = strlen(str);
	int   len_str_to_insert = strlen(str_to_insert);
	char *res_str = malloc(MAX_REPLACE_STR_LENGTH);

	if (res_str) {
		for (i = 0, k = 0; i < l && k < MAX_REPLACE_STR_LENGTH; i++)
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

char *get_file_matching_given_pattern_alloc(char *original_file,
                                            char *file_pattern)
{
	int   path_length = 0, filename_without_ext_length = 0;
	char *d = strrchr(original_file, '/');
	char *ext = (d ? strrchr(d, '.') : strrchr(original_file, '.'));
	char  path[256] = "", filename_without_ext[256] = "";
	char  new_file[256] = "";
	char *pattern = NULL;
	char *res_str = NULL;

	if (d != NULL) {
		path_length = d - original_file;
		if (ext != NULL)
			filename_without_ext_length = ext - d - 1;
		strncpy(filename_without_ext, d+1, filename_without_ext_length);
	}
	strncpy(path, original_file, path_length);

	if (filename_without_ext[0] != '\0') {
		pattern = replace_char_with_string_alloc(file_pattern, '$', filename_without_ext);
		/*printf("Pattern new = %s\nPattern old = %s\n", pattern, image_file_pattern);*/
	} else {
		pattern = file_pattern;
	}

	wdprintf(V_DEBUG, "util", "path = %s\n", path);
	if (get_first_matching_file_pattern_list(new_file, 256, path, pattern)) {
		res_str = malloc(256);
		if (res_str) {
			snprintf(res_str, 255, "%s/%s", path, new_file);
			res_str[255] = '\0';
		}
	}
	if (filename_without_ext[0] != '\0') free(pattern);
	return res_str;
}

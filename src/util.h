/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
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
void  strtoupper(char *target, const char *src, int len);
void  strtolower(char *target, const char *src, int len);
int   file_exists(char *filename);
int   file_copy(char *destination_file, char *source_file);
char *get_file_extension(char *filename);
int   get_first_matching_file(char *target, int target_length, char *path, char *pattern);
int   get_first_matching_file_pattern_list(char *target, int   target_length, 
                                           char *path,   char *pattern_list);
char *get_file_matching_given_pattern_alloc(char *original_file,
                                            char *file_pattern);
#endif

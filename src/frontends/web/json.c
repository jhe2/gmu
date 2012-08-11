/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: json.c  Created: 120811
 *
 * Description: JSON helper functions
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
#include "json.h"

char *json_string_escape_alloc(char *src)
{
	char *res = NULL;
	int   len = src ? strlen(src) : 0;

	if (len) {
		res = malloc(len*2); /* len*2 = worst case */
		if (res) {
			int i, j;
			for (i = 0, j = 0; src[i]; i++, j++) {
				if (src[i] < 32) {
					res[j] = 32;
				} else if (src[i] == '"' || src[i] == '\\') {
					res[j] = '\\';
					j++;
					res[j] = src[i];
				} else {
					res[j] = src[i];
				}
			}
			res[j] = '\0';
		}
	}
	return res;
}

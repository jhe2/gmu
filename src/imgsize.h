/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: imgsize.h  Created: 070614
 *
 * Description: Image size struct (used by jpeg.c and png.c)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#ifndef _IMGSIZE_H
#define _IMGSIZE_H
typedef struct _ImageSize
{
	int   file;
	FILE *infile;
	char *memptr;
	int   memsize;
} ImageSize;
#endif

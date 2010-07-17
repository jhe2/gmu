/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: png.h  Created: 070605
 *
 * Description: png image dimensions detector
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include "imgsize.h"
#ifndef _PNG_H
#define _PNG_H
/* functions return 1 if a valid png header was found, otherwise 0 */
int png_get_dimensions_from_file(ImageSize *is, char *filename, int *width, int *height);
int png_get_dimensions_from_memory(ImageSize *is, char *mem, int memsize, int *width, int *height);
#endif

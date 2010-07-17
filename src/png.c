/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: png.c  Created: 070605
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
#include <string.h>
#include "png.h"
#include "imgsize.h"

static int read_1_byte(ImageSize *is)
{
	int res = EOF;
	if (is->file) {
		res = fgetc(is->infile);
	} else if (is->memsize > 0) {
		res = (unsigned char)is->memptr++[0];
		is->memsize--;
	}
	return res;
}

static int check_png_header(ImageSize *is)
{
	int i, res = 1;
	const unsigned char pngsig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	for (i = 0; i < 8; i++) {
		if (read_1_byte(is) != pngsig[i]) {
			res = 0;
			break;
		}
	}
	return res;
}

static int get_png_image_dimensions(ImageSize *is, int *width, int *height)
{
	int   i, header_ok = 1;
	const unsigned char ihdr_header[] = { 73, 72, 68, 82 };
	unsigned char size[4];

	*width = *height = -1;

	if (check_png_header(is)) {
		for (i = 0; i < 4; i++)
			read_1_byte(is);
		for (i = 0; i < 4; i++) {
			if (read_1_byte(is) != ihdr_header[i]) {
				header_ok = 0;
				break;
			}
		}
		if (header_ok) {
			for (i = 0; i < 4; i++) size[i] = read_1_byte(is); /* width  */
			*width = size[0] * 256 * 256 * 256 + 
			         size[1] * 256 * 256 + size[2] * 256 + size[3];
			for (i = 0; i < 4; i++) size[i] = read_1_byte(is); /* height */
			*height = size[0] * 256 * 256 * 256 + 
			          size[1] * 256 * 256 + size[2] * 256 + size[3];
		}
	} else {
		header_ok = 0;
	}
	return header_ok;
}

int png_get_dimensions_from_file(ImageSize *is, char *filename, int *width, int *height)
{
	int res = 0;
	is->file = 1;
	is->infile = fopen(filename, "rb");
	if (is->infile) {
		res = get_png_image_dimensions(is, width, height);
		fclose(is->infile);
	}
	return res;
}

int png_get_dimensions_from_memory(ImageSize *is, char *mem, int memsize, int *width, int *height)
{
	int res = 0;
	is->memptr = mem;
	is->memsize = memsize;
	is->file = 0;
	if (is->memptr && is->memsize > 47)
		res = get_png_image_dimensions(is, width, height);
	return res;
}

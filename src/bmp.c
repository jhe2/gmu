/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: bmp.c  Created: 070616
 *
 * Description: bmp image dimensions detector
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bmp.h"
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

static int check_bmp_header(ImageSize *is)
{
	int i, res = 1;
	const unsigned char bmpsig[] = { 'B', 'M' };
	for (i = 0; i < 2; i++) {
		if (read_1_byte(is) != bmpsig[i]) {
			res = 0;
			break;
		}
	}
	for (i = 0; i < 12; i++)
		read_1_byte(is);
	return res;
}

static int int_pow(int x, int y)
{
	int i, r = x;
	for (i = 1; i < y; i++)
		r = r * x;
	if (y == 0) r = 1;
	return r;
}

static int calculate_int(unsigned char *four_bytes)
{
	int i, j, res = 0;
	for (i = 3; i >= 0; i--) {
		for (j = 0; j < 8; j++) {
			int p = int_pow(2, j);
			/*printf("%d:%d:%d\n", i, j, four_bytes[i] & p);*/
			if (i == 3 && (four_bytes[i] & 128))
				res -= (four_bytes[i] & p);
			else
				res += (four_bytes[i] & p) * int_pow(256, i);
		}
	}
	return res;
}

static int get_bmp_image_dimensions(ImageSize *is, int *width, int *height)
{
	int   i, header_ok = 1;
	unsigned char size[4];

	*width = *height = -1;

	if (check_bmp_header(is)) {
		for (i = 0; i < 4; i++)
			read_1_byte(is); /* skip size info */
		if (header_ok) {
			for (i = 0; i < 4; i++) size[i] = read_1_byte(is); /* width  */
			*width = calculate_int(size);
			for (i = 0; i < 4; i++) size[i] = read_1_byte(is); /* height */
			*height = calculate_int(size);
			if (*height < 0) *height = -*height;
		}
	} else {
		header_ok = 0;
	}
	return header_ok;
}

int bmp_get_dimensions_from_file(ImageSize *is, char *filename, int *width, int *height)
{
	int res = 0;
	is->file = 1;
	is->infile = fopen(filename, "rb");
	if (is->infile) {
		res = get_bmp_image_dimensions(is, width, height);
		fclose(is->infile);
	}
	return res;
}

int bmp_get_dimensions_from_memory(ImageSize *is, char *mem, int memsize, int *width, int *height)
{
	int res = 0;
	is->memptr = mem;
	is->memsize = memsize;
	is->file = 0;
	if (is->memptr && is->memsize > 47)
		res = get_bmp_image_dimensions(is, width, height);
	return res;
}

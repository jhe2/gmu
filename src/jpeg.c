/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: jpeg.h  Created: 070605
 *
 * Description: jpeg image dimensions detector
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include "jpeg.h"
#include "imgsize.h"
#include "debug.h"

#define M_SOF0  0xC0		/* Start Of Frame N */
#define M_SOF1  0xC1		/* N indicates which compression process */
#define M_SOF2  0xC2		/* Only SOF0-SOF2 are now in common use */
#define M_SOF3  0xC3
#define M_SOF5  0xC5		/* NB: codes C4 and CC are NOT SOF markers */
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */
#define M_APP0	0xE0		/* Application-specific marker, type N */
#define M_APP12	0xEC		/* (we don't bother to list all 16 APPn's) */
#define M_COM   0xFE		/* COMment */

static int read_1_byte(ImageSize *is)
{
	int c = EOF;
	if (is->file) {
		c = getc(is->infile);
	} else if (is->memsize > 0) {
		c = (unsigned char)is->memptr++[0];
		is->memsize--;
	}
	return c;
}

/* All 2-byte quantities in JPEG markers are MSB first */
static unsigned int read_2_bytes(ImageSize *is)
{
	int c1 = EOF, c2 = EOF;
	unsigned int res = EOF;

	if (is->file) {
		c1 = getc(is->infile);
		c2 = getc(is->infile);
	} else if (is->memsize > 1) {
		c1 = (unsigned char)is->memptr++[0];
		c2 = (unsigned char)is->memptr++[0];
		is->memsize -= 2;
	}
	if (c1 != EOF && c2 != EOF)
		res = (((unsigned int) c1) << 8) + ((unsigned int) c2);
	return res;
}

static int next_marker(ImageSize *is)
{
	int c;
	int discarded_bytes = 0;

	/* Find 0xFF byte; count and skip any non-FFs. */
	c = read_1_byte(is);
	while (c != 0xFF && c != EOF) {
		discarded_bytes++;
		c = read_1_byte(is);
	}
	/* Get marker code byte, swallowing any duplicate FF bytes.  Extra FFs
	 * are legal as pad bytes, so don't count them in discarded_bytes.
	 */
	if (c != EOF) {
		do {
			c = read_1_byte(is);
		} while (c == 0xFF);

		if (discarded_bytes != 0)
			wdprintf(V_WARNING, "jpeg", "Warning: Garbage data found in JPEG file.\n");
	} else {
		c = -1;
	}
	return c;
}

static int first_marker(ImageSize *is)
{
	int c1, c2;

	c1 = read_1_byte(is);
	c2 = read_1_byte(is);
	if (c1 != 0xFF || c2 != M_SOI)
		c2 = -1; /* Not a JPEG file */
	if (c2 == -1) wdprintf(V_WARNING, "jpeg", "WARNING: Not a jpeg image...\n");
	return c2;
}

/* Skip over an unknown or uninteresting variable-length marker */
static int skip_variable(ImageSize *is)
{
	int length;
	int res = 0;

	/* Get the marker parameter length count */
	length = read_2_bytes(is);
	/* Length includes itself, so must be at least 2 */
	if (length < 2 || length == EOF) {
		res = -1; /* error */
	} else {
		length -= 2;
		/* Skip over the remaining bytes */
		while (length > 0) {
			read_1_byte(is);
			length--;
		}
	}
	return res;
}

static void get_image_dimensions(ImageSize *is, int *width, int *height)
{
	unsigned int length;
	unsigned int image_height, image_width;
	int          num_components;
	int          ci;

	length = read_2_bytes(is);

	read_1_byte(is); /* data_precision */
	image_height = read_2_bytes(is);
	image_width = read_2_bytes(is);
	num_components = read_1_byte(is);

	*width = image_width;
	*height = image_height;

	if (length != (unsigned int) (8 + num_components * 3)) {
		*width = *height = -1;  /* Bogus SOF marker length */
	} else {
		for (ci = 0; ci < num_components; ci++) {
			read_1_byte(is);	/* Component ID code */
			read_1_byte(is);	/* H, V sampling factors */
			read_1_byte(is);	/* Quantization table number */
		}
	}
}

static int get_jpeg_image_dimensions(ImageSize *is, int *width, int *height)
{
	int marker = -1, jpeg_ok = 0;

	*width = *height = -1;

	if (is->infile || !is->file) {
		/* Expect SOI at start of file */
		if (first_marker(is) == M_SOI) {
			jpeg_ok = 1;
			/* Scan miscellaneous markers until we reach SOS. */
			for (; marker != M_SOS && marker != M_EOI; ) {
				marker = next_marker(is);
				switch (marker) {
				/* Note that marker codes 0xC4, 0xC8, 0xCC are not, and must not be,
				 * treated as SOFn.  C4 in particular is actually DHT.
				 */
				case M_SOF0:		/* Baseline */
				case M_SOF1:		/* Extended sequential, Huffman */
				case M_SOF2:		/* Progressive, Huffman */
				case M_SOF3:		/* Lossless, Huffman */
				case M_SOF5:		/* Differential sequential, Huffman */
				case M_SOF6:		/* Differential progressive, Huffman */
				case M_SOF7:		/* Differential lossless, Huffman */
				case M_SOF9:		/* Extended sequential, arithmetic */
				case M_SOF10:		/* Progressive, arithmetic */
				case M_SOF11:		/* Lossless, arithmetic */
				case M_SOF13:		/* Differential sequential, arithmetic */
				case M_SOF14:		/* Differential progressive, arithmetic */
				case M_SOF15:		/* Differential lossless, arithmetic */
					get_image_dimensions(is, width, height);
					break;
				default:			/* Anything else just gets skipped */
					if (skip_variable(is) < 0)
						return -1;
					break;
				}
			}
		}
	}
	return jpeg_ok;
}

int jpeg_get_dimensions_from_file(ImageSize *is, char *filename, int *width, int *height)
{
	int res = 0;
	is->file = 1;
	is->infile = fopen(filename, "rb");
	if (is->infile) {
		res = get_jpeg_image_dimensions(is, width, height);
		fclose(is->infile);
	}
	return res;
}

int jpeg_get_dimensions_from_memory(ImageSize *is, char *mem, int memsize, int *width, int *height)
{
	int res = 0;
	is->memptr = mem;
	is->memsize = memsize;
	is->file = 0;
	is->infile = NULL;
	if (is->memptr && is->memsize > 47)
		res = get_jpeg_image_dimensions(is, width, height);
	return res;
}

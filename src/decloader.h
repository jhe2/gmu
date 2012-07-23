/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: decloader.h  Created: 081022
 *
 * Description: Shared object decoader loader for Gmu audio decoders
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _DECLOADER_H
#define _DECLOADER_H
#include "gmudecoder.h"

typedef struct _DecoderChain DecoderChain;

struct _DecoderChain {
	DecoderChain *next;
	GmuDecoder   *gd;
};

GmuDecoder *decloader_load_decoder(char *so_file);
int         decloader_load_all(char *directory);
GmuDecoder *decloader_get_decoder_for_extension(char *file_extension);
GmuDecoder *decloader_get_decoder_for_mime_type(char *mime_type);
GmuDecoder *decloader_get_decoder_for_data_chunk(char *data, int size);
char       *decloader_get_all_extensions(void);
GmuDecoder *decloader_decoder_list_get_next_decoder(int getfirst);
void        decloader_free(void);
int         decloader_load_builtin_decoders(void);
#endif

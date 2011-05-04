/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: reader.h  Created: 110406
 *
 * Description: File/Stream reader functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _READER_H
#define _READER_H
#define MINIMUM_HTTP_READ 1024
#include <pthread.h>
#include "ringbuffer.h"
#include "wejpconfig.h"

#define HTTP_CACHE_SIZE 524288

typedef struct
{
	FILE *file;
	int   eof;
	int   seekable;
	long  file_size;

	int   sockfd;

	char *buf; /* Dynamic read buffer */
	int   buf_size;
	int   buf_data_size;

	ConfigFile streaminfo;

	RingBuffer rb_http;
	pthread_mutex_t mutex;
	pthread_t       thread;
} Reader;

/* Opens a local file or HTTP URL for reading */
Reader *reader_open(char *url);
int     reader_close(Reader *r);
int     reader_is_eof(Reader *r);
char    reader_read_byte(Reader *r);
int     reader_read_bytes(Reader *r, int size);
char   *reader_get_buffer(Reader *r);
int     reader_get_number_of_bytes_in_buffer(Reader *r);
/* Resets the stream to the beginning (if possible), returns 1 on success, 0 otherwise */
int     reader_reset_stream(Reader *r);
int     reader_seek(Reader *r, int byte_offset);
#endif

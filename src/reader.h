/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2021 Johannes Heimansberg (wej.k.vu)
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
#include <stdio.h>
#include <pthread.h>
#include "ringbuffer.h"
#include "wejconfig.h"

#define HTTP_CACHE_SIZE_MIN_KB 256
#define HTTP_CACHE_SIZE_MAX_KB 4096

typedef struct
{
	FILE           *file;
	int             eof;
	int             seekable;
	long            file_size;

	int             sockfd;

	char           *buf; /* Dynamic read buffer */
	size_t          buf_size;
	size_t          buf_data_size;

	ConfigFile     *streaminfo;

	RingBuffer      rb_http;
	pthread_mutex_t mutex;
	pthread_t       thread;

	unsigned long   stream_pos;

	int             is_ready;
} Reader;

/* Opens a local file or HTTP URL for reading */
int     reader_set_cache_size_kb(size_t size, size_t prebuffer_size);
int     reader_get_cache_fill(Reader *r);
Reader *reader_open(const char *url);
int     reader_close(Reader *r);
int     reader_is_ready(Reader *r);
int     reader_is_eof(Reader *r);
char    reader_read_byte(Reader *r);
int     reader_read_bytes(Reader *r, size_t size);
char   *reader_get_buffer(Reader *r);
size_t  reader_get_number_of_bytes_in_buffer(Reader *r);
/* Resets the stream to the beginning (if possible), returns 1 on success, 0 otherwise */
int     reader_reset_stream(Reader *r);
int     reader_is_seekable(Reader *r);
int     reader_seek_whence(Reader *r, long byte_offset, int whence);
int     reader_seek(Reader *r, long byte_offset);
long    reader_get_file_size(Reader *r);
unsigned long reader_get_stream_position(Reader *r);
/* Sets number of bytes in buffer to 0 */
void    reader_clear_buffer(Reader *r);
#endif

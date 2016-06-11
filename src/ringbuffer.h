/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: ringbuffer.h  Created: 060928
 *
 * Description: General purpose ring buffer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef WEJ_RINGBUFFER_H
#define WEJ_RINGBUFFER_H
#include <sys/types.h>

struct _RingBuffer {
	size_t  size;
	char   *buffer;
	size_t  read_ptr, write_ptr, buffer_fill, unread_fill;
	ssize_t unread_ptr;
};

typedef struct _RingBuffer RingBuffer;

int    ringbuffer_init(RingBuffer *rb, size_t size);
void   ringbuffer_free(RingBuffer *rb);
int    ringbuffer_write(RingBuffer *rb, const char *data, size_t size);
int    ringbuffer_read(RingBuffer *rb, char *target, size_t size);
size_t ringbuffer_get_fill(RingBuffer *rb);
size_t ringbuffer_get_free(RingBuffer *rb);
void   ringbuffer_clear(RingBuffer *rb);
size_t ringbuffer_get_size(RingBuffer *rb);
void   ringbuffer_set_unread_pos(RingBuffer *rb);
int    ringbuffer_unread(RingBuffer *rb);
#endif

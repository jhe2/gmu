/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
struct _RingBuffer {
	int   size;
	char *buffer;
	int   read_ptr, write_ptr, buffer_fill, unread_ptr, unread_fill;
};

typedef struct _RingBuffer RingBuffer;

int  ringbuffer_init(RingBuffer *rb, int size);
void ringbuffer_free(RingBuffer *rb);
int  ringbuffer_write(RingBuffer *rb, char *data, int size);
int  ringbuffer_read(RingBuffer *rb, char *target, int size);
int  ringbuffer_get_fill(RingBuffer *rb);
int  ringbuffer_get_free(RingBuffer *rb);
void ringbuffer_clear(RingBuffer *rb);
int  ringbuffer_get_size(RingBuffer *rb);
void ringbuffer_set_unread_pos(RingBuffer *rb);
int  ringbuffer_unread(RingBuffer *rb);
#endif

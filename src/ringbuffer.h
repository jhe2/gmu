/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: ringbuffer.h  Created: 060928
 *
 * Description: Audio ring buffer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
struct _RingBuffer {
	int   size;
	char *buffer;
	int   read_ptr, write_ptr, buffer_fill;
};

typedef struct _RingBuffer RingBuffer;

void ringbuffer_init(RingBuffer *rb, int size);
void ringbuffer_free(RingBuffer *rb);
int  ringbuffer_write(RingBuffer *rb, char *data, int size);
int  ringbuffer_read(RingBuffer *rb, char *target, int size);
int  ringbuffer_get_fill(RingBuffer *rb);
int  ringbuffer_get_free(RingBuffer *rb);
void ringbuffer_clear(RingBuffer *rb);
int  ringbuffer_get_size(RingBuffer *rb);

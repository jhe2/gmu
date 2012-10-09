/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: ringbuffer.c  Created: 060928
 *
 * Description: General purpose ring buffer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdlib.h>
#include "ringbuffer.h"
#include <stdio.h>

int ringbuffer_init(RingBuffer *rb, int size)
{
	rb->buffer      = (char *)malloc(size);
	rb->size        = size;
	rb->read_ptr    = 0;
	rb->write_ptr   = 0;
	rb->buffer_fill = 0;
	rb->unread_fill = 0;
	rb->unread_ptr  = -1;
	return rb->buffer ? 1 : 0;
}

void ringbuffer_free(RingBuffer *rb)
{
	if (rb->buffer != NULL) {
		free(rb->buffer);
		rb->buffer = NULL;
	}
}

void ringbuffer_clear(RingBuffer *rb)
{
	rb->read_ptr    = 0;
	rb->write_ptr   = 0;
	rb->buffer_fill = 0;
}

int ringbuffer_write(RingBuffer *rb, char *data, int size)
{
	int i = 0, result = 1;

	if (size > rb->size - rb->buffer_fill) {
		result = 0;
	} else {
		rb->buffer_fill += size;

		while (i < size && result == 1) {
			rb->buffer[rb->write_ptr] = data[i];
			i++;
			if (rb->write_ptr < rb->size - 1)
				rb->write_ptr++;
			else
				rb->write_ptr = 0;
		}
	}
	return result;
}

int ringbuffer_read(RingBuffer *rb, char *target, int size)
{
	int i = 0, result = 1;

	if (size > rb->buffer_fill) {
		result = 0;
	} else {
		rb->buffer_fill -= size;

		while (i < size) {
			target[i] = rb->buffer[rb->read_ptr];
			if (rb->read_ptr < rb->size - 1)
				rb->read_ptr++;
			else
				rb->read_ptr = 0;
			i++;
		}
	}
	return result;
}

int ringbuffer_get_fill(RingBuffer *rb)
{
	return rb->buffer_fill;
}

int ringbuffer_get_free(RingBuffer *rb)
{
	return rb->size - rb->buffer_fill;
}

int ringbuffer_get_size(RingBuffer *rb)
{
	return rb->size;
}

/* Remembers current ringbuffer read position for possible unrolling with unread. */
void ringbuffer_set_unread_pos(RingBuffer *rb)
{
	rb->unread_ptr = rb->read_ptr;
	rb->unread_fill = rb->buffer_fill;
}

/* Rolls back all reads since last ringbuffer_set_unread_pos() call.
 * No writes or other changes must have been made to the ring buffer inbetween. */
int ringbuffer_unread(RingBuffer *rb)
{
	int res = 0;
	if (rb->unread_ptr > -1) {
		rb->buffer_fill = rb->unread_fill;
		rb->read_ptr = rb->unread_ptr;
		rb->unread_ptr = -1;
		res = 1;
	}
	return res;
}

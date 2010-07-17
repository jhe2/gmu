/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: ringbuffer.c  Created: 060928
 *
 * Description: Audio ring buffer
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

void ringbuffer_init(RingBuffer *rb, int size)
{
	rb->buffer      = (char *)malloc(size);
	rb->size        = size;
	rb->read_ptr    = 0;
	rb->write_ptr   = 0;
	rb->buffer_fill = 0;
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

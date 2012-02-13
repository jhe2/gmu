/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: queue.h  Created: 120212
 *
 * Description: A queue for storing strings
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <pthread.h>

#ifndef GMU_QUEUE_H
#define GMU_QUEUE_H
typedef struct _QueueEntry QueueEntry;

struct _QueueEntry
{
	QueueEntry *next;
	char       *str;
};

struct _Queue
{
	QueueEntry     *first, *last;
	pthread_mutex_t mutex, mutex_cond;
	pthread_cond_t  cond;
};

typedef struct _Queue Queue;

void     queue_init(Queue *q);
int      queue_push(Queue *q, char *str);
char    *queue_pop_alloc(Queue *q);
void     queue_clear(Queue *q);
int      queue_is_empty(Queue *q);
//void     event_queue_wait_for_event(Queue *eq, int with_timeout);
void     queue_free(Queue *q);
#endif

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: eventqueue.h  Created: 090131
 *
 * Description: Gmu event queue
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <pthread.h>
#include "gmufrontend.h"

#ifndef _EVENTQUEUE_H
#define _EVENTQUEUE_H
typedef struct _EventQueueEntry EventQueueEntry;

struct _EventQueueEntry
{
	EventQueueEntry *next;
	GmuEvent         event;
	int              param;
};

struct _EventQueue
{
	EventQueueEntry *first, *last;
	pthread_mutex_t  mutex, mutex_cond;
	pthread_cond_t   cond;
};

typedef struct _EventQueue EventQueue;

void     event_queue_init(EventQueue *eq);
int      event_queue_push(EventQueue *eq, GmuEvent ev);
int      event_queue_push_with_parameter(EventQueue *eq, GmuEvent ev, int param);
/* Function to fetch the (optional) parameter that can be pushed with an event.
 * To be called before popping the actual event! */
int      event_queue_get_parameter(EventQueue *eq);
GmuEvent event_queue_pop(EventQueue *eq);
void     event_queue_clear(EventQueue *eq);
int      event_queue_is_event_waiting(EventQueue *eq);
void     event_queue_wait_for_event(EventQueue *eq, int with_timeout);
void     event_queue_free(EventQueue *eq);
#endif

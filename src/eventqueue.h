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

typedef struct _EventQueueEntry EventQueueEntry;

struct _EventQueueEntry
{
	EventQueueEntry *next;
	GmuEvent         event;
};

struct _EventQueue
{
	EventQueueEntry *first, *last;
	pthread_mutex_t  mutex;
};

typedef struct _EventQueue EventQueue;

void     event_queue_init(EventQueue *eq);
int      event_queue_push(EventQueue *eq, GmuEvent ev);
GmuEvent event_queue_pop(EventQueue *eq);
void     event_queue_clear(EventQueue *eq);
int      event_queue_is_event_waiting(EventQueue *eq);

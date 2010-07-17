/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: eventqueue.c  Created: 090131
 *
 * Description: Gmu event queue
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "eventqueue.h"

void event_queue_init(EventQueue *eq)
{
		eq->first = NULL;
		eq->last = NULL;
		pthread_mutex_init(&(eq->mutex), NULL);
}

int event_queue_push(EventQueue *eq, GmuEvent ev)
{
	EventQueueEntry *new_entry;
	int              result = 0;

	new_entry = malloc(sizeof(EventQueueEntry));
	pthread_mutex_lock(&(eq->mutex));
	if (new_entry) {
		new_entry->event = ev;
		new_entry->next = NULL;
		if (eq->last) eq->last->next = new_entry;
		eq->last = new_entry;
		if (!eq->first) eq->first = new_entry;
		result = 1;
	}
	pthread_mutex_unlock(&(eq->mutex));
	return result;
}

GmuEvent event_queue_pop(EventQueue *eq)
{
	GmuEvent result = GMU_NO_EVENT;

	pthread_mutex_lock(&(eq->mutex));
	if (eq->first) {
		EventQueueEntry *tmp = eq->first;

		result = eq->first->event;
		if (eq->first->next)
			eq->first = eq->first->next;
		else
			eq->first = NULL;
		if (eq->last == tmp) eq->last = NULL;
		free(tmp);
	}
	pthread_mutex_unlock(&(eq->mutex));
	return result;
}

void event_queue_clear(EventQueue *eq)
{
	while (event_queue_pop(eq) != GMU_NO_EVENT);
}

void event_queue_free(EventQueue *eq)
{
	event_queue_clear(eq);
	pthread_mutex_destroy(&(eq->mutex));
}

int event_queue_is_event_waiting(EventQueue *eq)
{
	return (eq->first ? 1 : 0);
}

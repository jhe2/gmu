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
#include <sys/time.h>
#include "eventqueue.h"

void event_queue_init(EventQueue *eq)
{
	eq->first = NULL;
	eq->last = NULL;
	pthread_mutex_init(&(eq->mutex), NULL);
	pthread_mutex_init(&(eq->mutex_cond), NULL);
	pthread_cond_init(&(eq->cond), NULL);
}

int event_queue_push_with_parameter(EventQueue *eq, GmuEvent ev, int param)
{
	EventQueueEntry *new_entry;
	int              result = 0;

	new_entry = malloc(sizeof(EventQueueEntry));
	pthread_mutex_lock(&(eq->mutex));
	if (new_entry) {
		new_entry->event = ev;
		new_entry->param = param;
		new_entry->next = NULL;
		if (eq->last) eq->last->next = new_entry;
		eq->last = new_entry;
		if (!eq->first) eq->first = new_entry;
		result = 1;
	}
	pthread_cond_broadcast(&(eq->cond));
	pthread_mutex_unlock(&(eq->mutex));
	return result;
}

int event_queue_push(EventQueue *eq, GmuEvent ev)
{
	return event_queue_push_with_parameter(eq, ev, 0);
}

int event_queue_get_parameter(EventQueue *eq)
{
	int p = 0;
	if (eq->first) {
		p = eq->first->param;
	}
	return p;
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
	pthread_cond_broadcast(&(eq->cond));
	pthread_mutex_destroy(&(eq->mutex));
	pthread_cond_destroy(&(eq->cond));
	pthread_mutex_destroy(&(eq->mutex_cond));
}

int event_queue_is_event_waiting(EventQueue *eq)
{
	int res;
	pthread_mutex_lock(&(eq->mutex));
	res = (eq->first ? 1 : 0);
	pthread_mutex_unlock(&(eq->mutex));
	return res;
}

static void timespec_add(struct timespec* a, struct timespec* b, struct timespec* out)
{
	time_t sec  = a->tv_sec + b->tv_sec;
	long   nsec = a->tv_nsec + b->tv_nsec;

	sec += nsec / 1000000000L;
	nsec = nsec % 1000000000L;

	out->tv_sec  = sec;
	out->tv_nsec = nsec;
}

/* with_timeout = timeout in milli seconds */
void event_queue_wait_for_event(EventQueue *eq, int with_timeout)
{
	pthread_mutex_lock(&(eq->mutex_cond));
	if (with_timeout <= 0) {
		pthread_cond_wait(&(eq->cond), &(eq->mutex_cond));
	} else {
		struct timespec ts, tsa, target_ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		tsa.tv_sec  = with_timeout / 1000;
		tsa.tv_nsec = (with_timeout % 1000) * 1000L * 1000L;
		timespec_add(&ts, &tsa, &target_ts);
		pthread_cond_timedwait(&(eq->cond), &(eq->mutex_cond), &target_ts);
	}
	pthread_mutex_unlock(&(eq->mutex_cond));
}

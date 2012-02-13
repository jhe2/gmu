/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: queue.c  Created: 120212
 *
 * Description: A queue for storing strings
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "queue.h"

void queue_init(Queue *q)
{
	q->first = NULL;
	q->last = NULL;
	pthread_mutex_init(&(q->mutex), NULL);
	pthread_mutex_init(&(q->mutex_cond), NULL);
	pthread_cond_init(&(q->cond), NULL);
}

int queue_push(Queue *q, char *str)
{
	QueueEntry *new_entry;
	int         result = 0;

	new_entry = malloc(sizeof(QueueEntry));
	pthread_mutex_lock(&(q->mutex));
	if (new_entry) {
		int len = str ? strlen(str) : 0;
		if (len > 0) {
			new_entry->str = malloc(len+1);
			if (new_entry->str) {
				memcpy(new_entry->str, str, len+1);
				new_entry->next = NULL;
				if (q->last) q->last->next = new_entry;
				q->last = new_entry;
				if (!q->first) q->first = new_entry;
				result = 1;
			}
		}
	}
	pthread_cond_broadcast(&(q->cond));
	pthread_mutex_unlock(&(q->mutex));
	return result;
}

char *queue_pop_alloc(Queue *q)
{
	char *result = NULL;

	pthread_mutex_lock(&(q->mutex));
	if (q->first) {
		QueueEntry *tmp = q->first;
		int         len = 0;

		len = q->first->str ? strlen(q->first->str) : 0;
		
		if (len > 0) {
			result = malloc(len+1);
			if (result) memcpy(result, q->first->str, len+1);
			free(q->first->str);
			if (q->first->next)
				q->first = q->first->next;
			else
				q->first = NULL;
			if (q->last == tmp) q->last = NULL;
		}
		free(tmp);
	}
	pthread_mutex_unlock(&(q->mutex));
	return result;
}

void queue_clear(Queue *q)
{
	char *str = NULL;
	while ((str = queue_pop_alloc(q)) != NULL)
		free(str);
}

void queue_free(Queue *q)
{
	queue_clear(q);
	pthread_cond_broadcast(&(q->cond));
	pthread_mutex_destroy(&(q->mutex));
	pthread_cond_destroy(&(q->cond));
	pthread_mutex_destroy(&(q->mutex_cond));
}

int queue_is_empty(Queue *q)
{
	int res;
	pthread_mutex_lock(&(q->mutex));
	res = (q->first ? 0 : 1);
	pthread_mutex_unlock(&(q->mutex));
	return res;
}

/*void event_queue_wait_for_event(EventQueue *eq, int with_timeout)
{
	pthread_mutex_lock(&(eq->mutex_cond));
	if (with_timeout <= 0) {
		pthread_cond_wait(&(eq->cond), &(eq->mutex_cond));
	} else {
		struct timespec ts;
		struct timeval  tp;
		gettimeofday(&tp, NULL);
		ts.tv_sec  = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		ts.tv_sec += with_timeout;
		pthread_cond_timedwait(&(eq->cond), &(eq->mutex_cond), &ts);
	}
	pthread_mutex_unlock(&(eq->mutex_cond));
}*/

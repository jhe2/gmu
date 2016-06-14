/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
 *
 * File: pthread_helper.c  Created: 160612
 *
 * Description: pthread related functions
 */
#include <pthread.h>
#include "pthread_helper.h"

int pthread_create_with_stack_size(
	pthread_t *thread, const size_t stack_size,
	void *(*start_routine) (void *), void *arg
)
{
	int                   res = -1;
	static pthread_attr_t attr;

	if (pthread_attr_init(&attr) == 0) {
		if (pthread_attr_setstacksize(&attr, stack_size) == 0) {
			res = pthread_create(thread, &attr, start_routine, arg);
		}
		pthread_attr_destroy(&attr);
	}
	return res;
}

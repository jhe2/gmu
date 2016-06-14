/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
 *
 * File: pthread_helper.h  Created: 160612
 *
 * Description: pthread related functions
 */
#ifndef PTHREAD_HELPER_H
#define PTHREAD_HELPER_H
#include <pthread.h>

int pthread_create_with_stack_size(
	pthread_t *thread, const size_t stack_size,
	void *(*start_routine) (void *), void *arg
);
#endif

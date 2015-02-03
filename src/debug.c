/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: debug.h  Created: 110107
 *
 * Description: Debug output
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "debug.h"

static Verbosity wdprintf_verbosity = V_DEBUG;

void wdprintf_set_verbosity(Verbosity v)
{
	wdprintf_verbosity = v;
}

int wdprintf(Verbosity v, char *module, char *fmt, ...)
{
	va_list    ap;
	char       timestr[200];
	time_t     t;
	struct tm *lt;

	if (v <= wdprintf_verbosity) {
		t  = time(NULL);
		lt = localtime(&t);
		if (lt && strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", lt) > 0)
			printf("%s ", timestr);
		if (module) printf("%s: ", module);
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
	return 0;
}

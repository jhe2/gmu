/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
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
#include "debug.h"

static Verbosity wdprintf_verbosity = V_DEBUG;

void wdprintf_set_verbosity(Verbosity v)
{
	wdprintf_verbosity = v;
}

int wdprintf(Verbosity v, char *module, char *fmt, ...)
{
	va_list ap;

	if (v <= wdprintf_verbosity) {
		if (module) printf("%s: ", module);
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
	return 0;
}

/*int main(void)
{
	wdprintf_set_verbosity(V_INFO);
	wdprintf(V_ERROR, "module", "Something is missing. Counted only %d items, expected %d\n", 3, 7);
	wdprintf(V_DEBUG, NULL, "Here we go... %c %d %d %f %f %c\n", 'X', 6, 6, 6.0, 6.0, 'z');
	return 0;
}*/

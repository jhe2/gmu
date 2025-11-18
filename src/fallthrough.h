/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2025 Johannes Heimansberg (wej.k.vu)
 *
 * File: fallthrough.h  Created: 251116
 *
 * Description: General purpose ring buffer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _FALLTHROUGH_H
#define _FALLTHROUGH_H

#ifdef __has_attribute
# if __has_attribute(__fallthrough__)
#  define fallthrough()                    __attribute__((__fallthrough__))
# endif
#endif
#ifndef fallthrough
# define fallthrough()                    do {} while (0)  /* fallthrough */
#endif

#endif

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2026 Johannes Heimansberg (wej.k.vu)
 *
 * File: reader_curl.c  Created: 070426
 *
 * Description: File/Stream reader functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

// This file is used by reader if URL_WITH_CURL is defined

#ifndef _READER_CURL_H
#define _READER_CURL_H

/* Opens a HTTP or HTTPS URL for reading */
Reader *reader_open_curl(Reader *r, const char *url, int max_redirects);

#endif

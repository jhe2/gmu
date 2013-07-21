/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: medialib.h  Created: 130627
 *
 * Description: Gmu media library
 */
#ifndef WEJ_MEDIALIB_H
#define WEJ_MEDIALIB_H
#include <sqlite3.h>
#include "trackinfo.h"

typedef struct GmuMedialib {
	sqlite3      *db;
	int           refresh_in_progress;
	sqlite3_stmt *pp_stmt_search;
} GmuMedialib;

typedef enum {
	GMU_MLIB_ANY, GMU_MLIB_ARTIST, GMU_MLIB_TITLE, GMU_MLIB_ALBUM, 
} GmuMedialibDataType;

int  medialib_open(GmuMedialib *gm);
void medialib_close(GmuMedialib *gm);
int  medialib_start_refresh(GmuMedialib *gm, void (*finished_callback)(void));
int  medialib_is_refresh_in_progress(GmuMedialib *gm);
void medialib_refresh(GmuMedialib *gm);
int  medialib_add_file(GmuMedialib *gm, char *file);
void medialib_path_add(GmuMedialib *gm, char *path);
void medialib_path_remove(GmuMedialib *gm, char *path);
/* Search the medialib; Returns the number of results */
int  medialib_search_find(GmuMedialib *gm, GmuMedialibDataType type, char *str);
TrackInfo medialib_search_fetch_next_result(GmuMedialib *gm);
void medialib_search_finish(GmuMedialib *gm);
#endif

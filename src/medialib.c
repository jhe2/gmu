/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: medialib.c  Created: 130627
 *
 * Description: Gmu media library
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>
#include "medialib.h"
#include "dirparser.h"
#include "trackinfo.h"
#include "metadatareader.h"
#include "util.h"
#include "debug.h"

int medialib_open(GmuMedialib *gm)
{
	int res = 0;
	gm->refresh_in_progress = 0;
	wdprintf(V_INFO, "medialib", "Opening medialib...\n");
	if (sqlite3_open_v2("gmu.db", &(gm->db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK) {
		wdprintf(V_ERROR, "medialib", "ERROR: Can't open database: %s\n", sqlite3_errmsg(gm->db));
		sqlite3_close(gm->db);
		gm->db = NULL;
	} else {
		res = 1;
		wdprintf(V_INFO, "medialib", "OK!\n");
	}
	return res;
}

void medialib_close(GmuMedialib *gm)
{
	if (gm->db) sqlite3_close(gm->db);
}

/*
 * Adds a single file (file = filename with full path) to the medialib
 * Returns 1 on success, 0 otherwise
 */
int medialib_add_file(GmuMedialib *gm, char *file)
{
	TrackInfo     ti;
	char          filetype[16];
	char         *tmp = get_file_extension(file);
	char         *q;
	sqlite3_stmt *pp_stmt = NULL;
	int           sqres;
	int           new_file = 1;

	filetype[0] = '\0';
	if (tmp != NULL)
		strtoupper(filetype, tmp, 15);

	wdprintf(V_DEBUG, "medialib", "file=%s type=%s\n", file, filetype);
	q = "SELECT id FROM track WHERE file = ?1 LIMIT 1";
	sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
	if (sqlite3_bind_text(pp_stmt, 1, file, -1, SQLITE_STATIC) == SQLITE_OK) {
		sqres = sqlite3_step(pp_stmt);
		if (sqres == SQLITE_ROW) {
			new_file = 0;
			wdprintf(V_DEBUG, "medialib", "File already in media library.\n");
		}
		sqlite3_finalize(pp_stmt);
	}
	
	trackinfo_init(&ti);
	if (new_file && metadatareader_read(file, filetype, &ti)) {
		/* Add file with metadata to media library... */
		char *q = "INSERT INTO track (file, artist, title, album, comment) VALUES (?1, ?2, ?3, ?4, ?5)";
		sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
		printf("prepare=%d\n", sqres);
		int a, b, c, d, e;
		a = sqlite3_bind_text(pp_stmt, 1, file,       -1, SQLITE_STATIC);
		b = sqlite3_bind_text(pp_stmt, 2, ti.artist,  -1, SQLITE_STATIC);
		c = sqlite3_bind_text(pp_stmt, 3, ti.title,   -1, SQLITE_STATIC);
		d = sqlite3_bind_text(pp_stmt, 4, ti.album,   -1, SQLITE_STATIC);
		e = sqlite3_bind_text(pp_stmt, 5, ti.comment, -1, SQLITE_STATIC);
		if (a == SQLITE_OK && b == SQLITE_OK && c == SQLITE_OK && d == SQLITE_OK && e == SQLITE_OK) {
			sqres = sqlite3_step(pp_stmt);
			if (sqres != SQLITE_DONE) {
				wdprintf(V_ERROR, "medialib", "ERROR while inserting into database: ERROR %d\n", sqres);
			}
		} else wdprintf(V_ERROR, "medialib", "NOT GOOD! Problem with SQL params.\n");
		sqlite3_finalize(pp_stmt);
		printf("%d %d %d %d %d\n", a, b, c, d, e);
	}
	return 0;
}

static int _medialib_add_file(void *gm, char *file)
{
	return medialib_add_file((GmuMedialib *)gm, file);
}

typedef struct gml_thread_params {
	GmuMedialib *gm;
	void       (*finished_callback)(void);
} gml_thread_params;

static void *thread_gml_refresh(void *udata)
{
	struct gml_thread_params *tp = (struct gml_thread_params *)udata;

	wdprintf(V_INFO, "medialib", "Refresh thread created.\n");
	medialib_refresh(tp->gm);
	wdprintf(V_INFO, "medialib", "Refresh thread finished.\n");
	tp->gm->refresh_in_progress = 0;
	if (tp->finished_callback) (tp->finished_callback)();
	return NULL;
}

int medialib_start_refresh(GmuMedialib *gm, void (*finished_callback)(void))
{
	static pthread_t          thread;
	static gml_thread_params  tp;
	int                       res = 0;

	if (!gm->refresh_in_progress) {
		gm->refresh_in_progress = 1;
		tp.gm = gm;
		tp.finished_callback = finished_callback;
		pthread_create(&thread, NULL, thread_gml_refresh, &tp);
		pthread_detach(thread);
		res = 1;
	}
	return res;
}

int medialib_is_refresh_in_progress(GmuMedialib *gm)
{
	return gm->refresh_in_progress;
}

void medialib_refresh(GmuMedialib *gm)
{
	sqlite3_stmt *pp_stmt = NULL;
	sqlite3_exec(gm->db, "BEGIN", 0, 0, 0);
	/* Fetch all medialib filesystem paths */
	if (sqlite3_prepare_v2(gm->db, "SELECT path FROM path", -1, &pp_stmt, NULL) == SQLITE_OK) {
		for (; sqlite3_step(pp_stmt) == SQLITE_ROW; ) {
			char *path = (char *)sqlite3_column_text(pp_stmt, 0);
			wdprintf(V_INFO, "medialib", "Scanning '%s'...\n", path);
			/* Scan path recursively... */
			dirparser_walk_through_directory_tree(path, _medialib_add_file, (void *)gm);
		}
	}
	sqlite3_finalize(pp_stmt);
	sqlite3_exec(gm->db, "COMMIT", 0, 0, 0);
}

void medialib_path_add(GmuMedialib *gm, char *path)
{
}

void medialib_path_remove(GmuMedialib *gm, char *path)
{
}

/* Search the medialib; Returns true on success; false (0) otherwise */
int medialib_search_find(GmuMedialib *gm, GmuMedialibDataType type, char *str)
{
	char *q;
	int   sqres;

	switch (type) {
		case GMU_MLIB_ANY:
		default:
			q = "SELECT * FROM track WHERE title LIKE ?1 OR artist LIKE ?1 OR album LIKE ?1";
			break;
		case GMU_MLIB_ARTIST:
			q = "SELECT * FROM track WHERE artist LIKE ?1";
			break;
		case GMU_MLIB_ALBUM:
			q = "SELECT * FROM track WHERE album LIKE ?1";
			break;
		case GMU_MLIB_TITLE:
			q = "SELECT * FROM track WHERE title LIKE ?1";
			break;
	}
	sqres = sqlite3_prepare_v2(gm->db, q, -1, &(gm->pp_stmt_search), NULL);
	if (sqres == SQLITE_OK) sqres = sqlite3_bind_text(gm->pp_stmt_search, 1, str, -1, SQLITE_STATIC);
	return (sqres == SQLITE_OK);
}

TrackInfo medialib_search_fetch_next_result(GmuMedialib *gm)
{
	TrackInfo ti;
	trackinfo_init(&ti);

	if (sqlite3_step(gm->pp_stmt_search) == SQLITE_ROW) {
		int   id     =         sqlite3_column_int(gm->pp_stmt_search, 0);
		char *file   = (char *)sqlite3_column_text(gm->pp_stmt_search, 1);
		char *artist = (char *)sqlite3_column_text(gm->pp_stmt_search, 3);
		char *title  = (char *)sqlite3_column_text(gm->pp_stmt_search, 4);
		char *album  = (char *)sqlite3_column_text(gm->pp_stmt_search, 5);
		trackinfo_set_trackid(&ti, id);
		trackinfo_set_filename(&ti, file);
		trackinfo_set_artist(&ti, artist);
		trackinfo_set_title(&ti, title);
		trackinfo_set_album(&ti, album);
	} else {
		trackinfo_set_trackid(&ti, -1);
	}
	return ti;
}

void medialib_search_finish(GmuMedialib *gm)
{
	sqlite3_finalize(gm->pp_stmt_search);
}

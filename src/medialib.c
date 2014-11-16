/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
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
#include "medialibsql.h"
#include "dirparser.h"
#include "trackinfo.h"
#include "metadatareader.h"
#include "util.h"
#include "debug.h"

int medialib_create_db_and_open(GmuMedialib *gm)
{
	int res = 0;
	if (sqlite3_open_v2("gmu.db", &(gm->db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) == SQLITE_OK) {
		res = sqlite3_exec(gm->db, medialib_sql, 0, 0, 0);
		wdprintf(V_DEBUG, "medialib", "Create result: %d\n", res);
		if (res == SQLITE_OK) res = 1;
	}
	return res;
}

int medialib_open(GmuMedialib *gm)
{
	int res = 0;
	gm->refresh_in_progress = 0;
	wdprintf(V_INFO, "medialib", "Opening medialib...\n");
	if (sqlite3_open_v2("gmu.db", &(gm->db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK) {
		wdprintf(V_ERROR, "medialib", "ERROR: Can't open database: %s\n", sqlite3_errmsg(gm->db));
		sqlite3_close(gm->db);
		gm->db = NULL;
		res = medialib_create_db_and_open(gm);
		if (res) wdprintf(V_INFO, "medialib", "New database created!\n");
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
int medialib_add_file(GmuMedialib *gm, const char *file)
{
	TrackInfo     ti;
	char          filetype[16];
	const char   *tmp = get_file_extension(file);
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
	
	trackinfo_init(&ti, 0);
	if (new_file && metadatareader_read(file, filetype, &ti)) {
		/* Add file with metadata to media library... */
		int   a, b, c, d, e;
		char *q = "INSERT INTO track (file, artist, title, album, comment) VALUES (?1, ?2, ?3, ?4, ?5)";

		sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
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
	}
	return 0;
}

static int _medialib_add_file(void *gm, const char *file)
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
			dirparser_walk_through_directory_tree(path, _medialib_add_file, (void *)gm, 0);
		}
	}
	sqlite3_finalize(pp_stmt);
	sqlite3_exec(gm->db, "COMMIT", 0, 0, 0);
}

void medialib_path_add(GmuMedialib *gm, const char *path)
{
	sqlite3_stmt *pp_stmt = NULL;
	int sqres;
	/* Check if path has already been added ... */
	char *q = "SELECT id FROM path WHERE path = ?1 LIMIT 1";
	sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
	if (sqres == SQLITE_OK) sqres = sqlite3_bind_text(pp_stmt, 1, path, -1, SQLITE_TRANSIENT);
	if (sqres == SQLITE_OK && sqlite3_step(pp_stmt) != SQLITE_ROW) {
		sqlite3_finalize(pp_stmt);
		q = "INSERT INTO path (path) VALUE (?1)";
		sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
		if (sqres == SQLITE_OK) sqres = sqlite3_bind_text(pp_stmt, 1, path, -1, SQLITE_TRANSIENT);
		sqlite3_step(pp_stmt);
	}
	sqlite3_finalize(pp_stmt);
}

void medialib_path_remove(GmuMedialib *gm, const char *path)
{
	sqlite3_stmt *pp_stmt = NULL;
	char *q = "DELETE FROM path WHERE path = ?1";
	int   sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
	if (sqres == SQLITE_OK) sqres = sqlite3_bind_text(pp_stmt, 1, path, -1, SQLITE_TRANSIENT);
	if (sqres == SQLITE_OK) sqlite3_step(pp_stmt);
	sqlite3_finalize(pp_stmt);
}

int medialib_path_list(GmuMedialib *gm)
{
	char *q = "SELECT path FROM path";
	int   sqres = sqlite3_prepare_v2(gm->db, q, -1, &(gm->pp_stmt_path_list), NULL);
	return (sqres == SQLITE_OK);
}

char *medialib_path_list_fetch_next_result(GmuMedialib *gm)
{
	char *path = NULL;
	if (sqlite3_step(gm->pp_stmt_path_list) == SQLITE_ROW) {
		path = (char *)sqlite3_column_text(gm->pp_stmt_path_list, 0);
	}
	return path;
}

void medialib_path_list_finish(GmuMedialib *gm)
{
	sqlite3_finalize(gm->pp_stmt_path_list);
}

/* Search the medialib; Returns true on success; false (0) otherwise */
int medialib_search_find(GmuMedialib *gm, GmuMedialibDataType type, const char *str)
{
	char *q;
	int   sqres = -1, len;
	char *str_tmp;

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
	len = str ? strlen(str) : 0;
	if (len > 0) {
		str_tmp = malloc(len+3);
		if (str_tmp) {
			snprintf(str_tmp, len+3, "%%%s%%", str);
			wdprintf(V_DEBUG, "medialib", "search str= %s\n", str_tmp);
			sqres = sqlite3_prepare_v2(gm->db, q, -1, &(gm->pp_stmt_search), NULL);
			if (sqres == SQLITE_OK) sqres = sqlite3_bind_text(gm->pp_stmt_search, 1, str_tmp, -1, SQLITE_TRANSIENT);
			free(str_tmp);
		}
	}
	return (sqres == SQLITE_OK);
}

TrackInfo medialib_search_fetch_next_result(GmuMedialib *gm)
{
	TrackInfo ti;
	trackinfo_init(&ti, 0);

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

/*
 * Browse the medialib by applying one or more filters.
 * sel_column is the column name to be selected, e.g. "album".
 * Last argument needs to be NULL.
 * For each filter two arguments are required:
 *  - column name (e.g. "artist") and
 *  - filter value (e.g. "Foo")
 * Any number of filters can be applied.
 */
int medialib_browse(GmuMedialib *gm, const char *sel_column, ...)
{
	char   *q = NULL, *qtmp = NULL;
	int     sqres = -1;
	va_list args;
	char   *arg;

	va_start(args, sel_column);

	qtmp = sqlite3_mprintf("SELECT DISTINCT %s FROM track WHERE 1=1", sel_column);
	for (arg = va_arg(args, char *); arg; arg = va_arg(args, char *)) {
		char *fcol = NULL;
		char *fvalue = va_arg(args, char *);
		if (fvalue) {
			if (strcmp(arg, "artist") == 0) {
				fcol = "artist";
			} else if (strcmp(arg, "album") == 0) {
				fcol = "album";
			} else if (strcmp(arg, "title") == 0) {
				fcol = "title";
			} else if (strcmp(arg, "date") == 0) {
				fcol = "date";
			}
			if (!fcol) break;
			q = sqlite3_mprintf("%s AND %s = %Q", qtmp, fcol, fvalue);
			if (qtmp) sqlite3_free(qtmp);
			qtmp = q;
		}
	}
	if (!q) q = qtmp;
	qtmp = sqlite3_mprintf("%s ORDER BY %s ASC", q, sel_column);
	sqlite3_free(q);
	q = qtmp;

	va_end(args);
	sqres = sqlite3_prepare_v2(gm->db, q, -1, &(gm->pp_stmt_browse), NULL);
	sqlite3_free(q);
	return (sqres == SQLITE_OK);
}

char *medialib_browse_fetch_next_result(GmuMedialib *gm)
{
	char *res = NULL;
	if (sqlite3_step(gm->pp_stmt_browse) == SQLITE_ROW) {
		res = (char *)sqlite3_column_text(gm->pp_stmt_browse, 0);
	}
	return res;
}

void medialib_browse_finish(GmuMedialib *gm)
{
	sqlite3_finalize(gm->pp_stmt_browse);
}

TrackInfo medialib_get_data_for_id(GmuMedialib *gm, int id)
{
	sqlite3_stmt *pp_stmt;
	TrackInfo     ti;
	char         *q = "SELECT * FROM track WHERE id = ?1 LIMIT 1";
	int           sqres;

	trackinfo_init(&ti, 0);

	sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
	if (sqres == SQLITE_OK) sqres = sqlite3_bind_int(pp_stmt, 1, id);
	if (sqres == SQLITE_OK && sqlite3_step(pp_stmt) == SQLITE_ROW) {
		char *file   = (char *)sqlite3_column_text(pp_stmt, 1);
		char *artist = (char *)sqlite3_column_text(pp_stmt, 3);
		char *title  = (char *)sqlite3_column_text(pp_stmt, 4);
		char *album  = (char *)sqlite3_column_text(pp_stmt, 5);
		trackinfo_set_trackid(&ti, id);
		trackinfo_set_filename(&ti, file);
		trackinfo_set_artist(&ti, artist);
		trackinfo_set_title(&ti, title);
		trackinfo_set_album(&ti, album);
	} else {
		trackinfo_set_trackid(&ti, -1);
	}
	sqlite3_finalize(pp_stmt);
	return ti;
}

static int rate_track(GmuMedialib *gm, int id, int relative, int rating)
{
	sqlite3_stmt *pp_stmt;
	char         *q;
	int           sqres;

	if (!relative) {
		q = "UPDATE track SET rating_explicit = ?2 WHERE id = ?1";
	} else {
		if (rating > 0)
			q = "UPDATE track SET rating_explicit = rating_explicit + 1 WHERE id = ?1";
		else
			q = "UPDATE track SET rating_explicit = rating_explicit - 1 WHERE id = ?1";
	}
	sqres = sqlite3_prepare_v2(gm->db, q, -1, &pp_stmt, NULL);
	if (sqres == SQLITE_OK) sqres = sqlite3_bind_int(pp_stmt, 1, id);
	if (!relative && sqres == SQLITE_OK) sqres = sqlite3_bind_int(pp_stmt, 2, rating);
	sqres = sqlite3_step(pp_stmt);
	if (sqres != SQLITE_DONE) {
		wdprintf(V_ERROR, "medialib", "ERROR while updating database: ERROR %d\n", sqres);
	}
	sqlite3_finalize(pp_stmt);
	return sqres;
}

int medialib_rate_track_up(GmuMedialib *gm, int id)
{
	return rate_track(gm, id, 1, 1);
}

int medialib_rate_track_down(GmuMedialib *gm, int id)
{
	return rate_track(gm, id, 1, -1);
}

int medialib_rate_track(GmuMedialib *gm, int id, int rating)
{
	return rate_track(gm, id, 0, rating);
}

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: sdl.c  Created: 060929
 *
 * Description: Gmu SDL frontend
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "SDL.h"
#include "SDL_image.h"
#include "../../util.h"
#include "../../gmufrontend.h"
#include "../../core.h"
#include "../../fileplayer.h"
#include "../../trackinfo.h"
#include "../../decloader.h"  /* for decoders list */
#include "../../gmudecoder.h" /* for decoders list */
#include "../../debug.h"

#include FILE_HW_H
#include "../../wejpconfig.h"
#include "../../pbstatus.h"

#include "textrenderer.h"
#include "filebrowser.h"
#include "plbrowser.h"
#include "coverviewer.h"
#include "kam.h"
#include "skin.h"
#include "textbrowser.h"
#include "playerdisplay.h"
#include "question.h"
#include "about.h"
#include "plmanager.h"
#include "inputconfig.h"
#include "help.h"

#define FPS          10
#define FRAME_SKIP    1
#define NOTICE_DELAY  8

#define TIMER_ELAPSED -1234

#define CPU_CLOCK_NORMAL       200
#define CPU_CLOCK_LOW_PLAYBACK 150

#define SCREEN_UPDATE_TIMER_ELAPSED 42

#define JOYSTICK_THRESHOLD 3200

static ConfigFile  *config = NULL;
Skin                skin;
static View         view, old_view;
static PlaylistBrowser pb;
static CoverViewer  cv;
static Question     dlg;
static SDL_TimerID  tid;

typedef enum Update { UPDATE_NONE = 0, UPDATE_DISPLAY = 2, UPDATE_HEADER = 4,
                      UPDATE_FOOTER = 8, UPDATE_TEXTAREA = 16, UPDATE_ALL = 2+4+8+16 } Update;
static Update       update = UPDATE_ALL;
static int          initialized = 0;

static char         base_dir[256];

typedef enum Quit { DONT_QUIT = 1, QUIT_WITH_ERROR, QUIT_WITHOUT_ERROR } Quit;
static Quit         quit = DONT_QUIT;

static GmuEvent    update_event = 0;

static int         fullscreen = 0;
static int         auto_select_cur_item = 1;
static int         screen_max_width = 0, screen_max_height = 0, screen_max_depth = 0;

static SDL_Surface *gmu_icon;

static void gmu_load_icon(void)
{
	Uint32 colorkey;

	gmu_icon = SDL_LoadBMP("gmu.bmp");
	if (gmu_icon) {
		colorkey = SDL_MapRGB(gmu_icon->format, 255, 0, 255);
		SDL_SetColorKey(gmu_icon, SDL_SRCCOLORKEY, colorkey);
		SDL_WM_SetIcon(gmu_icon, NULL);
	} else {
		wdprintf(V_WARNING, "sdl_frontend", "Window icon (gmu.bmp) not found or broken.\n");
	}
}

static void input_device_config(void)
{
	char tmp[256], *inputconf = NULL;
	inputconf = cfg_get_key_value(*config, "InputConfigFile");
	if (!inputconf) inputconf = "gmuinput.conf";
	snprintf(tmp, 255, "%s/%s", gmu_core_get_config_dir(), inputconf);
	input_config_init(tmp);
}

static SDL_Surface *init_sdl(int with_joystick, int width, int height, int fullscreen)
{
	SDL_Surface         *display = NULL;
	const SDL_VideoInfo *video_info;
	int                  init_okay = 0;

	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO | (with_joystick ? SDL_INIT_JOYSTICK : 0)) < 0) {
			wdprintf(V_ERROR, "sdl_frontend", "ERROR: Could not initialize SDL: %s\n", SDL_GetError());
		} else {
			wdprintf(V_DEBUG, "sdl_frontend", "SDL Video subsystem initialized.\n");
			init_okay = 1;
		}
	} else {
		wdprintf(V_ERROR, "sdl_frontend", "ERROR: SDL has already been initialized.\n");
	}

	if (init_okay) {
		video_info = SDL_GetVideoInfo();
		if (video_info) {
			screen_max_width  = video_info->current_w;
			screen_max_height = video_info->current_h;
			screen_max_depth  = video_info->vfmt->BitsPerPixel;
			wdprintf(V_INFO, "sdl_frontend", "Available screen real estate: %d x %d pixels @ %d bpp\n",
					 screen_max_width, screen_max_height, screen_max_depth);
		} else {
			screen_max_width  = 0;
			screen_max_height = 0;
			screen_max_depth  = 0;
			wdprintf(V_WARNING, "sdl_frontend", "Unable to determine screen resolution.\n");
		}

		width  = width  > screen_max_width  ? screen_max_width  : width;
		height = height > screen_max_height ? screen_max_height : height;
		width  = width  <= 0 ? 320 : width;
		height = height <= 0 ? 240 : height;

		if (fullscreen) {
			fullscreen = SDL_FULLSCREEN;
			width = screen_max_width;
			height = screen_max_height;
		}

		gmu_load_icon();
		display = SDL_SetVideoMode(width, height, screen_max_depth,
#ifndef SDLFE_NO_HWACCEL
								   SDL_HWSURFACE | SDL_HWACCEL |
#endif
								   SDL_RESIZABLE | fullscreen);
		if (display == NULL) {
			wdprintf(V_ERROR, "sdl_frontend", "ERROR: Could not initialize screen: %s\n", SDL_GetError());
			exit(1);
		}
#ifndef SHOW_MOUSE_CURSOR
		SDL_ShowCursor(0);
#endif
		if (with_joystick) {
			wdprintf(V_DEBUG, "sdl_frontend", "Opening joystick device.\n");
			SDL_JoystickOpen(0);
		}
		SDL_WM_SetCaption("Gmu", NULL);
		SDL_EnableUNICODE(1);
#ifdef HW_SDL_POST_INIT
		hw_sdl_post_init();
#endif
		wdprintf(V_INFO, "sdl_frontend", "SDL-Video init done.\n");
	}
	return display;
}

static Uint32 timer_callback(Uint32 interval, void *param)
{
	SDL_Event     event;
	SDL_UserEvent userevent;

	userevent.type  = SDL_USEREVENT;
	userevent.code  = SCREEN_UPDATE_TIMER_ELAPSED;
	userevent.data1 = NULL;
	userevent.data2 = NULL; 

	event.type = SDL_USEREVENT;
	event.user = userevent;

	SDL_PushEvent(&event);
	return interval;
}

static PB_Status play_file(char *file, TrackInfo *ti, CoverViewer *cv)
{
	gmu_core_play_file(file);
	return file_player_get_playback_status();
}

static int play_next(TrackInfo *ti, CoverViewer *cv)
{
	return gmu_core_next();
}

static int play_previous(TrackInfo *ti, CoverViewer *cv)
{
	return gmu_core_previous();
}

static int file_browser_process_action(FileBrowser *fb, PlaylistBrowser *pb, 
                                       TrackInfo   *ti, CoverViewer *cv,
                                       int          user_key_action,
                                       int          items_skip)
{
	Update update = UPDATE_NONE;
	char  *path = NULL;

	switch (user_key_action) {
		case FB_PLAY_FILE:
			if (!file_browser_selection_is_dir(fb)) {
				gmu_core_playlist_set_current(NULL);
				path = file_browser_get_selected_file_full_path_alloc(fb);
				if (path) {
					play_file(path, ti, cv);
					free(path);
				}
			}
			break;
		case FB_ADD_DIR_TO_PL:
			if (file_browser_selection_is_dir(fb)) {
				char *sf = file_browser_get_selected_file_full_path_alloc(fb);
				if (sf && gmu_core_playlist_add_dir(sf))
					player_display_set_notice_message("ADDING DIRECTORY...", NOTICE_DELAY);
				else
					player_display_set_notice_message("ALREADY ADDING A DIRECTORY", NOTICE_DELAY);
				update = UPDATE_ALL;
				if (sf) free(sf);
			} else {
				player_display_set_notice_message("NOT A DIRECTORY", NOTICE_DELAY);
			}
			break;
		case FB_NEW_PL_FROM_DIR:
			if (file_browser_selection_is_dir(fb)) {
				char *sf = file_browser_get_selected_file_full_path_alloc(fb);
				pl_browser_playlist_clear(pb);
				player_display_set_notice_message("CREATING NEW PLAYLIST...", NOTICE_DELAY);
				if (sf && gmu_core_playlist_add_dir(sf))
					player_display_set_notice_message("DIRECTORY ADDED", NOTICE_DELAY);
				update = UPDATE_ALL;
				if (sf) free(sf);
			} else {
				player_display_set_notice_message("NOT A DIRECTORY", NOTICE_DELAY);
			}
			break;
		case FB_DIR_UP:
			file_browser_change_dir(fb, "..");
			update = UPDATE_ALL;
			break;
		case FB_CHDIR:
			if (file_browser_selection_is_dir(fb)) {
				if (!file_browser_change_dir(fb, file_browser_get_selected_file(fb))) {
					wdprintf(V_WARNING, "sdl_frontend", "Failed to change directory. Even fallbacks did not work.\n");
				}
				update = UPDATE_ALL;
			}
			break;
		case FB_ADD_FILE_TO_PL_OR_CHDIR:
		case FB_INSERT_FILE_INTO_PL:
			if (file_browser_selection_is_dir(fb)) {
				if (!file_browser_change_dir(fb, file_browser_get_selected_file(fb))) {
					wdprintf(V_WARNING, "sdl_frontend", "Failed to change directory. Even fallbacks did not work.\n");
				}
				update = UPDATE_ALL;
			} else {
				char *sel_file = file_browser_get_selected_file(fb);
				if (sel_file) {
					path = file_browser_get_selected_file_full_path_alloc(fb);
					if (path) {
						if (user_key_action == FB_INSERT_FILE_INTO_PL) { /* insert item */
							Entry *sel_entry = gmu_core_playlist_get_first();
							int    i;

							wdprintf(V_DEBUG, "sdl_frontend", "Inserting entry after %d...\n", pl_browser_get_selection(pb));
							for (i = 0;
								 i < gmu_core_playlist_get_length() && i != pl_browser_get_selection(pb);
								 i++) {
								sel_entry = gmu_core_playlist_get_next(sel_entry);
							}
							gmu_core_playlist_insert_file_after(sel_entry, path);
							pl_brower_move_selection_down(pb);
							player_display_set_notice_message("ITEM INSERTED IN PLAYLIST", NOTICE_DELAY);
						} else { /* add item */
							gmu_core_playlist_add_file(path);
							player_display_set_notice_message("ITEM ADDED TO PLAYLIST", NOTICE_DELAY);
						}
						free(path);
					}
					if (file_browser_is_select_next_after_add(fb)) {
						file_browser_move_selection_down(fb);
						update = UPDATE_TEXTAREA;
					}
				}
			}
			break;
		case FB_DELETE_FILE: /* No longer supported */
			break;
		case MOVE_CURSOR_DOWN:
			file_browser_move_selection_n_items_down(fb, items_skip);
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_UP:
			file_browser_move_selection_n_items_up(fb, items_skip);
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_LEFT:
			file_browser_scroll_horiz(fb, -1);
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_RIGHT:
			file_browser_scroll_horiz(fb, +1);
			update = UPDATE_TEXTAREA;
			break;
		case PAGE_DOWN:
			file_browser_move_selection_n_items_down(fb, skin_textarea_get_number_of_lines((Skin *)fb->skin));
			update = UPDATE_TEXTAREA;
			break;
		case PAGE_UP:
			file_browser_move_selection_n_items_up(fb, skin_textarea_get_number_of_lines((Skin *)fb->skin));
			update = UPDATE_TEXTAREA;
			break;
	}
	return update;
}

struct _pb_delete_params {
	PlaylistBrowser *pb;
	CoverViewer     *cv;
	TrackInfo       *ti;
};

static void pb_delete_func(void *arg)
{
	struct _pb_delete_params *p = (struct _pb_delete_params *)arg;
	Entry                    *entry = gmu_core_playlist_get_first();
	int                       i = 0;
	if (entry != NULL) {
		char *f;
		while (i < pl_browser_get_selection(p->pb)) {
			entry = gmu_core_playlist_get_next(entry);
			i++;
		}
		if (pl_browser_are_selection_and_current_entry_equal(p->pb)) {
			if (!play_next(p->ti, p->cv)) {
				gmu_core_stop();
			}
		}
		f = gmu_core_playlist_get_entry_filename(entry);
		wdprintf(V_DEBUG, "sdl_frontend", "Delete file %s.\n", f);
		if (remove(f) == 0)
			player_display_set_notice_message("FILE DELETED!", NOTICE_DELAY);
		else
			player_display_set_notice_message("COULD NOT DELETE FILE!", NOTICE_DELAY);
		pl_browser_playlist_remove_selection(p->pb);
	}
}

static int playlist_browser_process_action(PlaylistBrowser *pb, TrackInfo *ti,
                                           CoverViewer     *cv,
                                           int              user_key_action,
                                           int              items_skip)
{
	Entry                           *entry;
	int                              i;
	Update                           update = UPDATE_NONE;
	static struct _pb_delete_params  pdp;
	static char                      buf[128];

	switch (user_key_action) {
		case PL_TOGGLE_RANDOM:
			gmu_core_playlist_cycle_play_mode();
			break;
		case PL_PLAY_ITEM:
			gmu_core_play_pl_item(pl_browser_get_selection(pb));
			update = UPDATE_ALL;
			break;
		case PL_CLEAR_PLAYLIST:
			pl_browser_playlist_clear(pb);
			player_display_set_notice_message("PLAYLIST CLEARED", NOTICE_DELAY);
			update = UPDATE_ALL;
			break;					
		case PL_REMOVE_ITEM:
			if (pl_browser_are_selection_and_current_entry_equal(pb)) {
				if (!play_next(ti, cv)) {
					gmu_core_stop();
				}
			}
			pl_browser_playlist_remove_selection(pb);
			update = UPDATE_ALL;
			break;
		case PL_DELETE_FILE:
			pdp.pb = pb;
			pdp.ti = ti;
			pdp.cv = cv;
			entry = gmu_core_playlist_get_first();
			i = 0;
			if (entry != NULL) {
				char buf2[128], *bufptr;
				if (gmu_core_playlist_get_play_mode() == PM_RANDOM ||
				    gmu_core_playlist_get_play_mode() == PM_RANDOM_REPEAT)
					gmu_core_playlist_reset_random();
				while (i < pl_browser_get_selection(pb)) {
					entry = gmu_core_playlist_get_next(entry);
					i++;
				}
				bufptr = strrchr(gmu_core_playlist_get_entry_filename(entry), '/')+1;
				if (pl_browser_get_filenames_charset(pb) != UTF_8) {
					charset_iso8859_1_to_utf8(buf2, bufptr, 127);
					bufptr = buf2;
				}
				snprintf(buf, 127, "Delete \"%s\"?", bufptr);
				question_set(&dlg, PLAYLIST, &view, buf, &pb_delete_func, (void *)&pdp);
				view = QUESTION;
				update = UPDATE_ALL;
			}
			break;
		case PL_SAVE_PLAYLIST:
			view = PLAYLIST_SAVE;
			update = UPDATE_ALL;
			break;
		case PL_ENQUEUE:
			gmu_core_playlist_entry_enqueue(pl_browser_get_selected_entry(pb));
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_DOWN:
			pl_brower_move_selection_n_items_down(pb, items_skip);
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_UP:
			pl_brower_move_selection_n_items_up(pb, items_skip);
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_LEFT:
			pl_browser_scroll_horiz(pb, -1);
			update = UPDATE_TEXTAREA;
			break;
		case MOVE_CURSOR_RIGHT:
			pl_browser_scroll_horiz(pb, +1);
			update = UPDATE_TEXTAREA;
			break;
		case PAGE_DOWN:
			pl_brower_move_selection_n_items_down(pb, skin_textarea_get_number_of_lines((Skin *)pb->skin));
			update = UPDATE_TEXTAREA;
			break;
		case PAGE_UP:
			pl_brower_move_selection_n_items_up(pb, skin_textarea_get_number_of_lines((Skin *)pb->skin));
			update = UPDATE_TEXTAREA;
			break;
	}
	return update;
}

static int cover_viewer_process_action(CoverViewer *cv, int user_key_action)
{
	Update update = UPDATE_NONE;
	switch (user_key_action) {
		case TRACKINFO_TOGGLE_COVER:
			cover_viewer_cycle_cover_and_spectrum_visibility(cv);
			update = UPDATE_TEXTAREA | UPDATE_HEADER;
			break;
		case TRACKINFO_TOGGLE_TEXT:
			cover_viewer_toggle_text_visible(cv);
			update = UPDATE_TEXTAREA | UPDATE_HEADER;
			break;
		case TRACKINFO_DELETE_FILE:
			wdprintf(V_INFO, "sdl_frontend", "Cannot delete file from here.\n");
			break;
		case MOVE_CURSOR_DOWN:
			cover_viewer_scroll_down(cv);
			update = UPDATE_TEXTAREA | UPDATE_HEADER;
			break;
		case MOVE_CURSOR_UP:
			cover_viewer_scroll_up(cv);
			update = UPDATE_TEXTAREA | UPDATE_HEADER;
			break;
		case MOVE_CURSOR_LEFT:
			cover_viewer_scroll_left(cv);
			update = UPDATE_TEXTAREA | UPDATE_HEADER;
			break;
		case MOVE_CURSOR_RIGHT:
			cover_viewer_scroll_right(cv);
			update = UPDATE_TEXTAREA | UPDATE_HEADER;
			break;
	}
	return update;
}

typedef struct M
{
	int so, st, d;
} M;

static void m_init(M *m)
{
	m->so = m->st = m->d = 0;
}

static View m_enable(M *m, int b, View v)
{
	static int s = 0;
	switch (s) {
		case 0: if (b == 273) s = 1;                         break;
		case 1: if (b == 273) s = 2;             else s = 0; break;
		case 2: if (b == 274) s = 3;             else s = 0; break;
		case 3: if (b == 274) s = 4;             else s = 0; break;
		case 4: if (b == 276) s = 5;             else s = 0; break;
		case 5: if (b == 275) s = 6;             else s = 0; break;
		case 6: if (b == 276) s = 7;             else s = 0; break;
		case 7: if (b == 275) s = 8;             else s = 0; break;
		case 8: if (b ==  98 || b == 308) s = 9; else s = 0; break;
		case 9: if (b ==  97 || b == 306) {
			v = EGG; s = 0; m->st = 0; m->so = 0; m->d = 0; break;
		}
	}
	return v;
}

static void m_read(M *m, int uka)
{
	switch (uka) {
		case MOVE_CURSOR_DOWN: m->d = (m->d == -1 ? 0 : 1); break;
		case MOVE_CURSOR_UP:   m->d = (m->d == 1 ? 0 : -1); break;
	}
}

static void m_draw(M *m, SDL_Surface *t)
{
	SDL_Rect   srect, drect;
	static int x = 50, y = 10, s = 8, dx = 0, dy = 0, py = 10, cy = 30;
	char       tmp[64];

	srect.w = skin.arrow_up->w;
	srect.h = skin.arrow_up->h;
	srect.x = 0; srect.y = 0;
	drect.x = gmu_widget_get_pos_x((GmuWidget *)&skin.lv, 1) + x;
	drect.y = gmu_widget_get_pos_y((GmuWidget *)&skin.lv, 1) + y;
	drect.w = 1; drect.h = 1;
	snprintf(tmp, 63, "%c0%cG%c(%d:%d%c", 80, 78, 32, m->so, m->st, ')');
	skin_draw_header_text(&skin, tmp, t);

	if (m->so < 3 && m->st < 3) {
		if (dx == 0 && dy == 0) { dx = s; dy = s; }
		if (skin.arrow_up) SDL_BlitSurface(skin.arrow_up, &srect, t, &drect);
		if (skin.arrow_down) {
			int i;

			srect.w = skin.arrow_down->w;
			srect.h = skin.arrow_down->h;
			srect.x = 0; srect.y = 0;
			drect.w = 1; drect.h = 1;
			if (py + m->d * 10 > 0 &&
				py + m->d * 10 < gmu_widget_get_height((GmuWidget *)&skin.lv, 1) - 3 * srect.h) {
				py += m->d * 10;
			} else {
				m->d = 0;
				if (py < 0) py = 0;
				if (py > gmu_widget_get_height((GmuWidget *)&skin.lv, 1) - 3 * srect.h)
					py = gmu_widget_get_height((GmuWidget *)&skin.lv, 1) - 3 * srect.h;
			}
			for (i = 0; i < 3; i++) {
				drect.x = gmu_widget_get_pos_x((GmuWidget *)&skin.lv, 1) + 32;
				drect.y = gmu_widget_get_pos_y((GmuWidget *)&skin.lv, 1) + py + i * srect.h;
				SDL_BlitSurface(skin.arrow_down, &srect, t, &drect);
				drect.x = gmu_widget_get_pos_x((GmuWidget *)&skin.lv, 1) + 
						  gmu_widget_get_width((GmuWidget *)&skin.lv, 1) - 32 - srect.w;
				drect.y = gmu_widget_get_pos_y((GmuWidget *)&skin.lv, 1) + cy + i * srect.h;
				SDL_BlitSurface(skin.arrow_down, &srect, t, &drect);
			}
		}
		x += dx; y += dy;
		if (dx > 0) {
			if (dy > 0 && y > cy && cy + 6 < gmu_widget_get_height((GmuWidget *)&skin.lv, 1) - 3 * srect.h) cy += 6;
			if (dy < 0 && y < cy && cy - 6 > 0) cy -= 6;
		}
		if (x >= gmu_widget_get_width((GmuWidget *)&skin.lv, 1) - skin.arrow_down->w - s - 32 - srect.w &&
			x <= gmu_widget_get_width((GmuWidget *)&skin.lv, 1) - skin.arrow_down->w - s - srect.w &&
			y >= cy - (srect.h >> 1) && y <= cy + 3 * srect.h + (srect.h >> 1)) {
			dx = -s;
		} else if (x > gmu_widget_get_width((GmuWidget *)&skin.lv, 1) - skin.arrow_down->w - s) {
			dx = -s; m->so++;
		}
		if (y > gmu_widget_get_height((GmuWidget *)&skin.lv, 1)- skin.arrow_down->h - s) dy = -s;
		if (x <= 32 + srect.w && x > 32 && y >= py - srect.h && y <= py + 4 * srect.h) {
			dx = s;
		} else if (x < s) {
			dx = s; m->st++; update |= UPDATE_HEADER;
		}
		if (y < s) dy = s;
	} else {
		if (m->so > m->st) snprintf(tmp, 63, "%c %c %c", 53, 61, 66);
		else snprintf(tmp, 63, "%c%cM%c%c%cV%cR", 71, 65, 69, 32, 48, 69);
		textrenderer_draw_string_with_highlight(&skin.font1, &skin.font2, tmp, 0, t,
		                                        (gmu_widget_get_width((GmuWidget *)&skin.lv, 1) - 
		                                        textrenderer_get_string_length(tmp) * skin.font1.chwidth) >> 1,
		                                        gmu_widget_get_pos_y((GmuWidget *)&skin.lv, 1) +
		                                        (gmu_widget_get_height((GmuWidget *)&skin.lv, 1) >> 1),
		                                        63, RENDER_DEFAULT);
	}
}

static SDL_Surface *display = NULL;

static void run_player(char *skin_name, char *decoders_str)
{
	SDL_Surface     *buffer = NULL;
	SDL_Event        event;

	FileBrowser      fb;
	TextBrowser      tb_about, tb_help;
	PlaylistManager  ps;
	M                m;

	int              button = -1;
	int              modifier = 0, hold_state = 0, allow_volume_control_in_hold_state = 0;
	int              time_remaining = 0;
	int              update_display = 1, display_inactive = 0;
	int              button_repeat_timer = -1, items_skip = 1, frame_skip_counter = FRAME_SKIP;
	int              seconds_until_backlight_poweroff = 0, backlight_poweroff_timer = -1;
	int              backlight_poweron_on_track_change = 0;
	int              seek_step = 10;

	KeyActionMapping kam[LAST_ACTION];
	int              user_key_action = -1;
	TrackInfo       *ti = gmu_core_get_current_trackinfo_ref();

	{
		char *val;

		val = cfg_get_key_value(*config, "AutoSelectCurrentPlaylistItem");
		auto_select_cur_item = (val && strncmp(val, "yes", 3) == 0) ? 1 : 0;

		val = cfg_get_key_value(*config, "TimeDisplay");
		time_remaining = (val && strncmp(val, "remaining", 9) == 0) ? 1 : 0;

		val = cfg_get_key_value(*config, "BacklightPowerOnOnTrackChange");
		backlight_poweron_on_track_change = (val && strncmp(val, "yes", 3) == 0) ? 1 : 0;

		old_view = view = FILE_BROWSER;
		val = cfg_get_key_value(*config, "FirstRun");
		if (val && strncmp(val, "yes", 3) == 0)
			view = HELP;
	}
	m_init(&m);

	quit = DONT_QUIT;
	/* Initialize and load button mapping */
	{
		char tmp[256], *keymap_file;
		key_action_mapping_init(kam);
		keymap_file = cfg_get_key_value(*config, "KeyMap");
		if (keymap_file) {
			snprintf(tmp, 255, "%s/%s", gmu_core_get_config_dir(), keymap_file);
			if (!key_action_mapping_load_config(kam, tmp)) {
				quit = QUIT_WITH_ERROR;
				wdprintf(V_ERROR, "sdl_frontend", "Error while loading keymap config.\n");
			}
		} else {
			quit = QUIT_WITH_ERROR;
			wdprintf(V_ERROR, "sdl_frontend", "No keymap file specified.\n");
		}
	}

	if (!skin_init(&skin, skin_name)) {
		quit = QUIT_WITH_ERROR;
		wdprintf(V_ERROR, "sdl_frontend", "skin_init() reported an error.\n");
	}

	if (quit == DONT_QUIT) {
		SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, display->w,
		                                        display->h, display->format->BitsPerPixel,
		                                        0, 0, 0, 0);
		buffer = SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);

		question_init(&dlg, &skin);

		dir_set_ext_filter(gmu_core_get_file_extensions(), 1);
		if (strncmp(cfg_get_key_value(*config, "FileSystemCharset"), "UTF-8", 5) == 0) {
			char *base_dir = cfg_get_key_value(*config, "SDL_frontend.BaseDir");
			file_browser_init(&fb, &skin, UTF_8, base_dir ? base_dir : "/");
			pl_browser_init(&pb, &skin, UTF_8);
			charset_filename_set(UTF_8);
		} else {
			char *base_dir = cfg_get_key_value(*config, "SDL_frontend.BaseDir");
			file_browser_init(&fb, &skin, ISO_8859_1, base_dir ? base_dir : "/");
			pl_browser_init(&pb, &skin, ISO_8859_1);
			charset_filename_set(ISO_8859_1);
		}

		{
			int   directories_first = 0, select_next_after_add = 0;
			char *tmp;

			directories_first = strncmp(cfg_get_key_value(*config, 
														  "FileBrowserFoldersFirst"),
														  "yes", 3) == 0 ? 1 : 0;
			file_browser_set_directories_first(&fb, directories_first);
			tmp = cfg_get_key_value(*config, "DefaultFileBrowserPath");
			if (tmp) file_browser_change_dir(&fb, tmp);
			tmp = cfg_get_key_value(*config, "SDL_frontend.FileBrowserSelectNextAfterAdd");
			if (tmp) {
				select_next_after_add = strncmp(tmp, "yes", 3) == 0 ? 1 : 0;
				file_browser_select_next_after_add(&fb, select_next_after_add);
			}
		}

		about_init(&tb_about, &skin, decoders_str);
		help_init(&tb_help, &skin, kam);

		cover_viewer_init(&cv, &skin, 
		                  strncmp(cfg_get_key_value(*config, "CoverArtworkLarge"), "yes", 3) == 0 ? 
		                  1 : 0,
		                  strncmp(cfg_get_key_value(*config, "SmallCoverArtworkAlignment"), "left", 4) == 0 ?
		                  ALIGN_LEFT : ALIGN_RIGHT,
		                  strncmp(cfg_get_key_value(*config, "LoadEmbeddedCoverArtwork"), "first", 5) == 0 ? 
		                  EMBEDDED_COVER_FIRST : 
		                  (strncmp(cfg_get_key_value(*config, "LoadEmbeddedCoverArtwork"), "last", 4) == 0 ?
		                  EMBEDDED_COVER_LAST : EMBEDDED_COVER_NO));
		plmanager_init(&ps, cfg_get_key_value(*config, "PlaylistSavePresets"), &skin);

		if (strncmp(cfg_get_key_value(*config, "Scroll"), "auto", 3) == 0)
			player_display_set_scrolling(SCROLL_AUTO);
		if (strncmp(cfg_get_key_value(*config, "Scroll"), "always", 3) == 0)
			player_display_set_scrolling(SCROLL_ALWAYS);
		if (strncmp(cfg_get_key_value(*config, "Scroll"), "never", 3) == 0)
			player_display_set_scrolling(SCROLL_NEVER);
		player_display_set_notice_message("GMU "VERSION_NUMBER, 10);

		if (strncmp(cfg_get_key_value(*config, "AllowVolumeControlInHoldState"), "yes", 3) == 0)
			allow_volume_control_in_hold_state = 1;

		seconds_until_backlight_poweroff = 
			atoi(cfg_get_key_value(*config, "SecondsUntilBacklightPowerOff"));
		seconds_until_backlight_poweroff = (seconds_until_backlight_poweroff > 0 ? 
		                                    seconds_until_backlight_poweroff : -1);

		backlight_poweroff_timer = seconds_until_backlight_poweroff * FPS;
		tid = SDL_AddTimer(1000 / FPS, timer_callback, NULL);

		if (gmu_core_playlist_get_length() > 0) {
			if (view != HELP) view = PLAYLIST;
			update = UPDATE_ALL;
		}
		initialized = 1;
		wdprintf(V_DEBUG, "sdl_frontend", "Initialization successful.\n");
	} else {
		wdprintf(V_WARNING, "sdl_frontend", quit == QUIT_WITHOUT_ERROR ?
		                                    "Strange. Exit was requested, shutting down." :
		                                    "An error was detected, shutting down.");
	}

	while (quit == DONT_QUIT && SDL_WaitEvent(&event)) {
		switch (event.type) {
			case SDL_VIDEORESIZE: {
				SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE,
				                                        event.resize.w, event.resize.h,
				                                        display->format->BitsPerPixel, 0, 0, 0, 0);
				SDL_FreeSurface(buffer);
				buffer = SDL_DisplayFormat(tmp);
				SDL_FreeSurface(tmp);
				display = SDL_SetVideoMode(event.resize.w, event.resize.h,
				                           buffer->format->BitsPerPixel,
#ifndef SDLFE_NO_HWACCEL
				                           SDL_HWSURFACE | SDL_HWACCEL |
#endif
				                           SDL_RESIZABLE);
				if (!display) {
					wdprintf(V_FATAL, "sdl_frontend", "Unable to set new video mode.\n");
					exit(-2); /* should not happen */
				}
				update = UPDATE_ALL;
				break;
			}
			case SDL_KEYUP:
			case SDL_JOYBUTTONUP:
				switch (event.type) {
					case SDL_KEYUP:       button = event.key.keysym.sym; break;
					case SDL_JOYBUTTONUP: button = event.jbutton.button; break;
					default: break;
				}

				if (modifier && key_action_mapping_get_action(kam, button, 0, view, ACTIVATE_PRESS) == MODIFIER) {
					modifier = 0;
					update = UPDATE_FOOTER;
				}
				button_repeat_timer = -1;
				seek_step = 10;
				break;
			case SDL_KEYDOWN:
			case SDL_JOYBUTTONDOWN:
				button_repeat_timer = 5;
				break;
			case SDL_JOYAXISMOTION: {
				static int last = 0;
				int        joy_axis_dir = 0;

				/*wdprintf(V_DEBUG, "sdl_frontend", "Joy axis motion: %d (axis %d)\n", event.jaxis.value, event.jaxis.axis);*/
				if (event.jaxis.value >= JOYSTICK_THRESHOLD && joy_axis_dir <= 0) {
					joy_axis_dir =  1;
				} else if (event.jaxis.value <= -JOYSTICK_THRESHOLD && joy_axis_dir >= 0) {
					joy_axis_dir = -1;
				} else if (event.jaxis.value > -JOYSTICK_THRESHOLD && event.jaxis.value < JOYSTICK_THRESHOLD) {
					joy_axis_dir = 0;
					button_repeat_timer = -1;
				}
				if (joy_axis_dir == last)
					continue; /* Eat the event */
				else
					button_repeat_timer = 5;
				last = joy_axis_dir;
				break;
			}
			case SDL_JOYHATMOTION:
				wdprintf(V_DEBUG, "sdl_frontend", "Joy Hat motion\n");
				break;
			case SDL_QUIT:
				gmu_core_quit();
				break;
			case SDL_ACTIVEEVENT: {
				/* Stop screen update when Gmu is invisible/minimized/whatever */
				if (SDL_GetAppState() & SDL_APPACTIVE) {
					display_inactive = 0;
				} else {
					display_inactive = 1;
					backlight_poweroff_timer = seconds_until_backlight_poweroff * FPS;
				}
				break;
			}
			default:
				break;
		}

		if (button_repeat_timer == 0) {
			backlight_poweroff_timer = seconds_until_backlight_poweroff * FPS;
			items_skip = 2;
		} else {
			items_skip = 1;
		}

		if (event.type == SDL_KEYDOWN || event.type == SDL_JOYBUTTONDOWN ||
		    event.type == SDL_KEYUP   || event.type == SDL_JOYBUTTONUP   ||
		    event.type == SDL_JOYAXISMOTION ||
		    (button_repeat_timer == 0 && user_key_action > 0)) {
			ActivateMethod amethod = ACTIVATE_PRESS;

			switch (event.type) {
				case SDL_KEYUP:			amethod = ACTIVATE_RELEASE;
				case SDL_KEYDOWN:       button = event.key.keysym.sym; break;
				case SDL_JOYBUTTONUP:	amethod = ACTIVATE_RELEASE;
				case SDL_JOYBUTTONDOWN: button = event.jbutton.button; break;
				case SDL_JOYAXISMOTION: {
					int joy_axis_dir = 0;
					amethod = ACTIVATE_JOYAXIS_MOVE;
					if (event.jaxis.value >= JOYSTICK_THRESHOLD && joy_axis_dir <= 0) {
						joy_axis_dir =  1;
						button = (event.jaxis.axis+1) * joy_axis_dir;
					} else if (event.jaxis.value <= -JOYSTICK_THRESHOLD && joy_axis_dir >= 0) {
						joy_axis_dir = -1;
						button = (event.jaxis.axis+1) * joy_axis_dir;
					} else if (event.jaxis.value > -JOYSTICK_THRESHOLD && event.jaxis.value < JOYSTICK_THRESHOLD) {
						joy_axis_dir = 0;
						button = 0;
					}
					break;
				}
				default: break;
			}

			/*printf("sdl_frontend: button=%d char=%c method=%d brt=%d\n", 
			       button, event.key.keysym.unicode <= 127 && 
			               event.key.keysym.unicode >= 32 ? event.key.keysym.unicode : '?',
			               amethod, button_repeat_timer);*/

			/* Reinitialize random seed each time a button is pressed: */
			srand(time(NULL));

			if (backlight_poweroff_timer == TIMER_ELAPSED && !hold_state) {
				/* Restore background: */
				skin_update_bg(&skin, display, buffer);
				update_display = 1;
				hw_display_on();
			}

			backlight_poweroff_timer = seconds_until_backlight_poweroff * FPS;

			if (button_repeat_timer != 0) {
				user_key_action = key_action_mapping_get_action(kam, button, modifier, view, amethod);
			}
			if (button_repeat_timer == 0) {
				/* Disable button repeat for everything but scroll actions,
				 * volume control and seeking: */
				if (user_key_action != MOVE_CURSOR_UP   &&
				    user_key_action != MOVE_CURSOR_DOWN &&
				    user_key_action != MOVE_CURSOR_LEFT &&
				    user_key_action != MOVE_CURSOR_RIGHT &&
				    user_key_action != PAGE_UP &&
				    user_key_action != PAGE_DOWN &&
					user_key_action != GLOBAL_INC_VOLUME &&
					user_key_action != GLOBAL_DEC_VOLUME &&
					user_key_action != GLOBAL_SEEK_FWD &&
					user_key_action != GLOBAL_SEEK_BWD)
				    user_key_action = -1;
			}

			switch (user_key_action) {
				case MODIFIER:
					button_repeat_timer = -1;
					modifier = 1;
					update = UPDATE_FOOTER;
					break;
				case GLOBAL_LOCK:
					if (!hold_state) {
						hw_display_off();
						update_display = 0;
						/* Clear the whole screen: */
						SDL_FillRect(display, NULL, 0);
						SDL_UpdateRect(display, 0, 0, 0, 0);
					} else {
						/* Restore background: */
						skin_update_bg(&skin, display, buffer);
						update_display = 1;
						hw_display_on();
					}
					hold_state = !hold_state;
					break;
				case GLOBAL_UNLOCK:
					if (hold_state) {
						/* Restore background: */
						skin_update_bg(&skin, display, buffer);
						update_display = 1;
						hw_display_on();
						hold_state = 0;
					}
					break;
			}

			if (!hold_state) {
				switch (user_key_action) { /* global actions */
					case GLOBAL_TOGGLE_VIEW:
						if (view == TRACK_INFO) {
							view = FILE_BROWSER;
							cover_viewer_disable_spectrum_analyzer(&cv);
						} else if (view == PLAYLIST) {
							view = TRACK_INFO;
							cover_viewer_enable_spectrum_analyzer(&cv);
						} else {
							view = PLAYLIST;
							cover_viewer_disable_spectrum_analyzer(&cv);
						}
						update = UPDATE_ALL;
						break;
					case GLOBAL_STOP:
						gmu_core_stop();
						update = UPDATE_ALL;
						break;
					case GLOBAL_PROGRAM_INFO:
						if (view != ABOUT && view != HELP) old_view = view;
						view = ABOUT;
						update = UPDATE_ALL;
						break;
					case GLOBAL_HELP:
						if (view != HELP && view != ABOUT) old_view = view;
						view = HELP;
						update = UPDATE_ALL;
						break;
					case GLOBAL_NEXT:
						if (play_next(ti, &cv)) {
							if (auto_select_cur_item)
								pl_browser_set_selection(&pb, gmu_core_playlist_get_current_position());
						} else {
							player_display_set_notice_message("CANNOT JUMP TO NEXT TRACK", NOTICE_DELAY);
						}
						update = UPDATE_ALL;
						break;
					case GLOBAL_PREV:
						if (play_previous(ti, &cv)) {
							if (auto_select_cur_item)
								pl_browser_set_selection(&pb, gmu_core_playlist_get_current_position());
						} else {
							player_display_set_notice_message("CANNOT JUMP TO PREV TRACK", NOTICE_DELAY);
						}
						update = UPDATE_ALL;
						break;
					case GLOBAL_PAUSE:
						if (file_player_get_playback_status())
							gmu_core_play_pause();
						SDL_Delay(50);
						break;
					case GLOBAL_SEEK_FWD:
						seek_step = (seek_step < 60 ? seek_step+1 : seek_step);
						file_player_seek(seek_step);
						break;
					case GLOBAL_SEEK_BWD:
						seek_step = (seek_step < 60 ? seek_step+1 : seek_step);
						file_player_seek(-1 * seek_step);
						break;
					case GLOBAL_EXIT:
						gmu_core_quit();
						break;
					case GLOBAL_TOGGLE_TIME:
						time_remaining = !time_remaining;
						break;
					case GLOBAL_FULLSCREEN: {
						int          w, h;
						SDL_Surface *tmp;
						static int   prev_w = 320, prev_h = 240;

						fullscreen = !fullscreen;
						if (fullscreen) {
							prev_w = display->w;
							prev_h = display->h;
							w = screen_max_width;
							h = screen_max_height;
						} else {
							w = prev_w;
							h = prev_h;
						}
						tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h,
				                                   display->format->BitsPerPixel, 0, 0, 0, 0);
						SDL_FreeSurface(buffer);
						buffer = SDL_DisplayFormat(tmp);
						SDL_FreeSurface(tmp);
						display = SDL_SetVideoMode(w, h, buffer->format->BitsPerPixel,
#ifndef SDLFE_NO_HWACCEL
												   SDL_HWSURFACE | SDL_HWACCEL |
#endif
												   SDL_RESIZABLE | (fullscreen ? SDL_FULLSCREEN : 0));
						if (!display) {
							wdprintf(V_FATAL, "sdl_frontend", "Unable to set new video mode.\n");
							exit(-2); /* should not happen */
						}
						update = UPDATE_ALL;
						break;
					}
					case GLOBAL_SET_SHUTDOWN_TIMER: {
						char *timer_msg = NULL;
						switch (gmu_core_get_shutdown_time_total()) {
							default:
							case 0: /* Timer disabled */
								gmu_core_set_shutdown_time(15);
								timer_msg = "SHUT DOWN IN 15 MINUTES";
								break;
							case 15: /* 15 minutes */
								gmu_core_set_shutdown_time(30);
								timer_msg = "SHUT DOWN IN 30 MINUTES";
								break;
							case 30:
								gmu_core_set_shutdown_time(60);
								timer_msg = "SHUT DOWN IN 60 MINUTES";
								break;
							case 60:
								gmu_core_set_shutdown_time(90);
								timer_msg = "SHUT DOWN IN 90 MINUTES";
								break;
							case 90:
								gmu_core_set_shutdown_time(-1);
								timer_msg = "SHUT DOWN AFTER LAST TRACK";
								break;
							case -1: /* Shutdown after last track */
								gmu_core_set_shutdown_time(0);
								timer_msg = "SHUT DOWN TIMER DISABLED";
								break;
						}
						player_display_set_notice_message(timer_msg, NOTICE_DELAY);
						break;
					}
				}
				if (amethod == ACTIVATE_PRESS) view = m_enable(&m, button, view);

				switch (view) { /* view specific actions */
					case FILE_BROWSER:
						update |= file_browser_process_action(&fb, &pb, ti, &cv, user_key_action, items_skip);
						break;				
					case PLAYLIST:
						update |= playlist_browser_process_action(&pb, ti, &cv, user_key_action, items_skip);
						break;
					case ABOUT:
						if (about_process_action(&tb_about, &view, old_view, user_key_action))
							update |= UPDATE_TEXTAREA | UPDATE_HEADER | UPDATE_FOOTER;
						break;
					case HELP:
						if (help_process_action(&tb_help, &view, old_view, user_key_action))
							update |= UPDATE_TEXTAREA | UPDATE_HEADER | UPDATE_FOOTER;
						break;
					case TRACK_INFO:
						update |= cover_viewer_process_action(&cv, user_key_action);
						break;
					case QUESTION:
						question_process_action(&dlg, user_key_action);
						update = UPDATE_ALL;
						break;
					case PLAYLIST_SAVE:
						plmanager_process_action(&ps, &view, user_key_action);
						switch(plmanager_get_flag(&ps)) {
							char buf23[64], buf42[64], temp[256];

							case PLMANAGER_SAVE_LIST:
								plmanager_reset_flag(&ps);
								snprintf(temp, 255, "%s/%s", base_dir, plmanager_get_selection(&ps));
								wdprintf(V_INFO, "sdl_frontend", "Playlist file: %s\n", temp);
								strtoupper(buf42, plmanager_get_selection(&ps), 63);
								if (gmu_core_export_playlist(temp)) {
									snprintf(buf23, 63, "SAVED AS %s\n", buf42);
								} else {
									snprintf(buf23, 63, "FAILED SAVING %s\n", buf42);
								}
								player_display_set_notice_message(buf23, NOTICE_DELAY);
								break;
							case PLMANAGER_LOAD_LIST:
								gmu_core_playlist_clear();
							case PLMANAGER_APPEND_LIST:
								plmanager_reset_flag(&ps);
								snprintf(temp, 255, "%s/%s", base_dir, plmanager_get_selection(&ps));
								gmu_core_add_m3u_contents_to_playlist(temp);
								player_display_set_notice_message("M3U ADDED TO PLAYLIST", NOTICE_DELAY);
								break;
							default:
								break;
						}
						update = UPDATE_ALL;
						break;
					case EGG:
						m_read(&m, user_key_action);
						update |= UPDATE_TEXTAREA | UPDATE_HEADER | UPDATE_FOOTER;
						break;
					default:
						break;
				}
			}
			if (!hold_state || allow_volume_control_in_hold_state) {
				switch (user_key_action) {
					case GLOBAL_INC_VOLUME:
						gmu_core_set_volume(gmu_core_get_volume()+1);
						break;
					case GLOBAL_DEC_VOLUME:
						gmu_core_set_volume(gmu_core_get_volume()-1);
						break;
				}
			}
		}

		if (event.type == SDL_USEREVENT) {
			if (update_event == GMU_TRACKINFO_CHANGE || update_event == GMU_PLAYMODE_CHANGE) {
				update = UPDATE_ALL;
				update_event = GMU_NO_EVENT;
			}

			if (event.type != SDL_KEYDOWN && event.type != SDL_JOYBUTTONDOWN &&
		        event.type != SDL_KEYUP && event.type != SDL_JOYBUTTONUP) {
				if (button_repeat_timer > 0)      button_repeat_timer--;
				if (!display_inactive && backlight_poweroff_timer > 0)
					backlight_poweroff_timer--;
			}

			if (backlight_poweroff_timer == 0) {
				if (!display_inactive) {
					hw_display_off();
					/* Clear the whole screen: */
					SDL_FillRect(display, NULL, 0);
					SDL_UpdateRect(display, 0, 0, 0, 0);
					update_display = 0;
					backlight_poweroff_timer = TIMER_ELAPSED;
				}
			}
			if (frame_skip_counter > 0)
				frame_skip_counter--;
			else
				update |= UPDATE_DISPLAY;

			/* When in trackinfo view, update view every few seconds 
			 * or more often when spectrum analyzer has been enabled */
			if (view == TRACK_INFO && !(update & UPDATE_TEXTAREA)) {
				static int cnt = 0;
				if (cnt == 30 || cover_viewer_is_spectrum_analyzer_enabled(&cv)) {
					update |= UPDATE_TEXTAREA;
					cnt = 0;
				}
				cnt++;
			}
			if (view == PLAYLIST &&
			    gmu_core_playlist_is_recursive_directory_add_in_progress())
				update |= UPDATE_TEXTAREA | UPDATE_HEADER;
			if (view == EGG) update |= UPDATE_TEXTAREA | UPDATE_HEADER;
		}

		if (update != UPDATE_NONE && update_display && !display_inactive) {
			char buf[128];

			if ((update & UPDATE_DISPLAY) && frame_skip_counter == 0) {
				frame_skip_counter = FRAME_SKIP;
				skin_draw_display_bg(&skin, buffer);
				player_display_draw(&skin.font_display, ti,
									(file_player_get_item_status() == STOPPED ? STOPPED : 
									 (gmu_core_playback_is_paused() ? PAUSED : PLAYING)),
									file_player_playback_get_time(), time_remaining,
									(10 * gmu_core_get_volume()) / (gmu_core_get_volume_max()-1),
									gmu_core_playlist_is_recursive_directory_add_in_progress(),
									gmu_core_get_shutdown_time_remaining(),
									buffer);
				skin_update_display(&skin, display, buffer);
			}

			if (update & UPDATE_FOOTER) {
				skin_draw_footer_bg(&skin, buffer);
				key_action_mapping_generate_help_string(kam, buf, 127, modifier, view);
				skin_draw_footer_text(&skin, buf, buffer);
				skin_update_footer(&skin, display, buffer);
			}

			if (update & UPDATE_HEADER)
				skin_draw_header_bg(&skin, buffer);

			if (update & UPDATE_TEXTAREA) {
				skin_draw_textarea_bg(&skin, buffer);
				switch (view) {
					case FILE_BROWSER:
						file_browser_draw(&fb, buffer);
						break;
					case PLAYLIST:
						pl_browser_draw(&pb, buffer);
						break;
					case ABOUT:
						text_browser_draw(&tb_about, buffer);
						break;
					case HELP:
						text_browser_draw(&tb_help, buffer);
						break;
					case TRACK_INFO:
						if (file_player_is_metadata_loaded()) cover_viewer_update_data(&cv, ti);
						cover_viewer_show(&cv, buffer, ti);
						break;
					case QUESTION:
						question_draw(&dlg, buffer);
						break;
					case PLAYLIST_SAVE:
						plmanager_draw(&ps, buffer);
						break;
					case EGG:
						m_draw(&m, buffer);
						break;
					default:
						break;
				}
				skin_update_textarea(&skin, display, buffer);
			}

			if (update & UPDATE_HEADER)
				skin_update_header(&skin, display, buffer);

			update = UPDATE_NONE;
		}
	}

	if (tid) SDL_RemoveTimer(tid);
	input_config_free();

	if (quit != QUIT_WITH_ERROR) {
		if (strncmp(cfg_get_key_value(*config, "RememberSettings"), "yes", 3) == 0) {
			char val[64];
			
			wdprintf(V_INFO, "sdl_frontend", "Saving settings...\n");
			cfg_add_key(config, "TimeDisplay", time_remaining ? 
			                                   "remaining" : "elapsed");
			if (buffer) {
				snprintf(val, 63, "%d", buffer->w);
				cfg_add_key(config, "SDL_frontend.Width", val);
				snprintf(val, 63, "%d", buffer->h);
				cfg_add_key(config, "SDL_frontend.Height", val);
				SDL_FreeSurface(buffer);
			}
			cfg_add_key(config, "SDL_frontend.Fullscreen", fullscreen ? "yes" : "no");
		}
		file_browser_free(&fb);
		cover_viewer_free(&cv);
		skin_free(&skin);
	} else {
		gmu_core_quit();
	}
}

static void *start_player(void *arg)
{
	int             start = 1, setup = 0;
	char            skin_name[128] = "";

	if (!getcwd(base_dir, 255)) start = 0;

	if (start || setup) {
		if (skin_name[0] == '\0') {
			char *skinname = cfg_get_key_value(*config, "DefaultSkin");
			if (skinname) strncpy(skin_name, skinname, 127);
			skin_name[127] = '\0';
		}
	}
	/*if (setup) {
		start = run_setup(config, skin_name);
		strncpy(skin_name, cfg_get_key_value(*config, "DefaultSkin"), 127);
		skin_name[127] = '\0';
	}*/
	if (start) {
		char       *decoders_str = NULL;
		GmuDecoder *gd = NULL;

		/* SDL_EnableKeyRepeat(200, 80); */

		/* Prepare list of loaded decoders for the about dialog */
		gd = decloader_decoder_list_get_next_decoder(1);
		while (gd) {
			const char *tmp = (*gd->get_name)();
			int         len = 0, len_tmp = 0;

			if (tmp) len_tmp = strlen(tmp);
			if (len_tmp > 0) {
				if (decoders_str) len = strlen(decoders_str);
				decoders_str = realloc(decoders_str, (decoders_str ? strlen(decoders_str) : 0) + len_tmp + 8);
			}

			snprintf(decoders_str+len, len_tmp + 4, "- %s\n", tmp);
			decoders_str[len+len_tmp+3] = '\0';
			gd = decloader_decoder_list_get_next_decoder(0);
		}
		if (decoders_str == NULL)
			run_player(skin_name, "No decoders have been loaded.");
		else
			run_player(skin_name, decoders_str);
		if (decoders_str) free(decoders_str);
	}
	wdprintf(V_DEBUG, "sdl_frontend", "start_player() done.\n");
	return 0;
}

static pthread_t fe_thread;

static int init(void)
{
	int   w = 320, h = 240, res = 0;
	char *val;
	SDL_Surface *ds;

	config = gmu_core_get_config();
	fullscreen = 0;
	val = cfg_get_key_value(*config, "SDL_frontend.Width");
	if (val) w = atoi(val);
	val = cfg_get_key_value(*config, "SDL_frontend.Height");
	if (val) h = atoi(val);
	val = cfg_get_key_value(*config, "SDL_frontend.Fullscreen");
	if (val && strncmp(val, "yes", 3) == 0) fullscreen = 1;
	input_device_config();
	ds = init_sdl(input_config_has_joystick(), w, h, fullscreen);
	if (!ds) {
		wdprintf(V_ERROR, "sdl_frontend", "ERROR: Display surface uninitialized.\n");
	} else {
		wdprintf(V_INFO, "sdl_frontend", "Display surface initialized.\n");
		display = ds;
		if (pthread_create(&fe_thread, NULL, start_player, NULL) == 0)
			res = 1;
	}
	return res;
}

static void shut_down(void)
{
	wdprintf(V_DEBUG, "sdl_frontend", "Shutting down now!\n");
	quit = QUIT_WITHOUT_ERROR;
	if (pthread_join(fe_thread, NULL) == 0)
		wdprintf(V_DEBUG, "sdl_frontend", "Thread stopped.\n");
	else
		wdprintf(V_ERROR, "sdl_frontend", "ERROR stopping thread.\n");
	wdprintf(V_DEBUG, "sdl_frontend", "Closing SDL video subsystem...\n");
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	if (gmu_icon) SDL_FreeSurface(gmu_icon);
	wdprintf(V_INFO, "sdl_frontend", "All done.\n");
}

static const char *get_name(void)
{
	return "Gmu SDL frontend v0.9";
}

static int event_callback(GmuEvent event, int param)
{
	TrackInfo *ti;

	/*printf("sdl_frontend: Got event: %d\n", event);*/
	switch (event) {
		case GMU_QUIT:
			quit = QUIT_WITHOUT_ERROR;
			break;
		case GMU_TRACKINFO_CHANGE:
			if (initialized) {
				ti = gmu_core_get_current_trackinfo_ref();
				if (tid) SDL_RemoveTimer(tid);
				cover_viewer_update_data(&cv, ti);
				if (strcmp(cfg_get_key_value(*config, "EnableCoverArtwork"), "yes") == 0)
					cover_viewer_load_artwork(&cv, ti, trackinfo_get_file_name(ti), 
											  cfg_get_key_value(*config, "CoverArtworkFilePattern"),
											  (int *)&update);
				update_event = event;
				tid = SDL_AddTimer(1000 / FPS, timer_callback, NULL);
				if (auto_select_cur_item)
					pl_browser_set_selection(&pb, gmu_core_playlist_get_current_position());
			}
			break;
		case GMU_VOLUME_CHANGE: {
			char volnotice[20];
			snprintf(volnotice, 19, "VOLUME: %d/%d", gmu_core_get_volume(), gmu_core_get_volume_max());
			player_display_set_notice_message(volnotice, NOTICE_DELAY);
			break;
		}
		case GMU_BUFFERING:
			player_display_set_notice_message("BUFFERING...", NOTICE_DELAY);
			player_display_set_playback_symbol_blinking(1);
			break;
		case GMU_BUFFERING_DONE:
		case GMU_BUFFERING_FAILED:
			player_display_set_playback_symbol_blinking(0);
			break;
		case GMU_PLAYMODE_CHANGE: {
			char *notice_msg = NULL;
			switch (gmu_core_playlist_get_play_mode()) {
				case PM_CONTINUE:
					notice_msg = "PLAYMODE: CONTINUE";
					break;
				case PM_RANDOM:
					notice_msg = "PLAYMODE: RANDOM";
					break;
				case PM_RANDOM_REPEAT:
					notice_msg = "PLAYMODE: RANDOM+REPEAT";
					break;
				case PM_REPEAT_1:
					notice_msg = "PLAYMODE: REPEAT TRACK";
					break;
				case PM_REPEAT_ALL:
					notice_msg = "PLAYMODE: REPEAT ALL";
					break;
			}
			if (notice_msg)
				player_display_set_notice_message(notice_msg, NOTICE_DELAY);
			update_event = event;
			break;
		}
		default:
			break;
	}
	return 0;
}

static GmuFrontend gf = {
	"SDL_frontend",
	get_name,
	init,
	shut_down,
	NULL,
	event_callback,
	NULL
};

GmuFrontend *GMU_REGISTER_FRONTEND(void)
{
	return &gf;
}

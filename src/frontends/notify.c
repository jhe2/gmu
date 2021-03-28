/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2021 Johannes Heimansberg (wej.k.vu)
 *
 * File: notify.c  Created: 170311
 *
 * Description: Gmu song change notification plugin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../gmufrontend.h"
#include "../core.h"
#include "../fileplayer.h"
#include "../debug.h"
#include <gio/gio.h>

static int notify_enabled = 0;

static const char *get_name(void)
{
	return "Gmu Notify Plugin v0.2";
}

static int init(void)
{
	ConfigFile *cf = gmu_core_get_config();

	gmu_core_config_acquire_lock();
	cfg_add_key_if_not_present(cf, "Notify.Enable", "yes");
	cfg_key_add_presets(cf, "Notify.Enable", "yes", "no", NULL);
	if (cfg_get_boolean_value(cf, "Notify.Enable")) {
		wdprintf(V_INFO, "notify", "Initializing notify plugin.\n");
		notify_enabled = 1;
	} else {
		wdprintf(V_INFO, "notify", "Notify plugin has been disabled.\n");
	}
	gmu_core_config_release_lock();
	return notify_enabled;
}

static void notify_trackinfo(TrackInfo *ti)
{
	if (ti && trackinfo_acquire_lock(ti)) {
		char           title[256];
		GApplication  *application;
		GNotification *notification;
		GIcon         *icon;

		if (trackinfo_get_channels(ti) > 0) {
			application = g_application_new("gmu.music.player", G_APPLICATION_FLAGS_NONE);
			if (application) {
				icon = g_themed_icon_new("dialog-information");

				trackinfo_get_full_title(ti, title, 256);

				notification = g_notification_new(title);
				if (notification) {
					g_notification_set_body(notification, trackinfo_get_album(ti));
					if (icon) g_notification_set_icon(notification, icon);
					g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_LOW);

					g_application_register(application, NULL, NULL);
					g_application_send_notification(application, NULL, notification);
					g_object_unref(notification);
				} else {
					wdprintf(V_WARNING, "notify", "WARNING: Failed to create notification.");
				}
				if (icon) g_object_unref(icon);
				g_object_unref(application);
			} else {
				wdprintf(V_WARNING, "notify", "WARNING: Failed to create notification.");
			}
		}
		trackinfo_release_lock(ti);
	}
}

static int event_callback(GmuEvent event, int param)
{
	if (notify_enabled) {
		switch (event) {
			case GMU_QUIT:
				break;
			case GMU_TRACKINFO_CHANGE:
				notify_trackinfo(gmu_core_get_current_trackinfo_ref());
				break;
			case GMU_PLAYBACK_STATE_CHANGE:
				break;
			default:
				break;
		}
	}
	return 0;
}

static GmuFrontend gf = {
	"notify",
	get_name,
	init,
	NULL,
	NULL,
	event_callback,
	NULL
};

GmuFrontend *GMU_REGISTER_FRONTEND(void)
{
	return &gf;
}

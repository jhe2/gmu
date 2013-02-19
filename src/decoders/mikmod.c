/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: mikmod.c  Created: 090810
 *
 * Description: Mikmod module decoder
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mikmod.h>
#include "../gmudecoder.h"
#include "../trackinfo.h"
#include "../util.h"
#include "../debug.h"
#define BUF_SIZE 32768

static MODULE *module;

static const char *get_name(void)
{
	return "Mikmod module decoder v0.1";
}

static int open_file(char *filename)
{
	int   res = 0;
	CHAR *list;

	md_mixfreq = 44100; 
	md_mode = DMODE_16BITS; 
	md_mode |= DMODE_STEREO; 
	md_device = 0; 
	md_volume = 96; 
	md_musicvolume = 128; 
	md_sndfxvolume = 128; 
	md_pansep = 128; 
	md_reverb = 0; 
	md_mode |= DMODE_SOFT_MUSIC|DMODE_SURROUND;

	wdprintf(V_DEBUG, "mikmod", "Init.\n");

	list = MikMod_InfoDriver();
	if (list)
		free(list);
	else
		MikMod_RegisterDriver(&drv_nos);
	list = MikMod_InfoLoader();
	if (list) {
		free(list);
	} else {
		MikMod_RegisterLoader(&load_it);
		MikMod_RegisterLoader(&load_xm);
		MikMod_RegisterLoader(&load_s3m);
		MikMod_RegisterLoader(&load_stm);
		MikMod_RegisterLoader(&load_ult);
		MikMod_RegisterLoader(&load_669);
		MikMod_RegisterLoader(&load_mod);
		MikMod_RegisterLoader(&load_m15);
	}

	res = MikMod_Init(NULL);
	if (res) {
  		wdprintf(V_ERROR, "mikmod", "Init failed: %s\n", MikMod_strerror(MikMod_errno));
	}
	wdprintf(V_INFO, "mikmod", "Loading %s...\n", filename);
	module = Player_Load(filename, 64, 0);
	if (module) {
		/*char *filename_without_path = NULL;*/

		Player_Start(module);

		/*filename_without_path = strrchr(ti->file_name, '/');
		if (filename_without_path != NULL)
			filename_without_path++;
		else
			filename_without_path = ti->file_name;

		if (strlen(ti->title) == 0) {
			filename_without_path = charset_filename_convert_alloc(filename_without_path);
			strncpy(ti->title, filename_without_path, SIZE_TITLE-1);
			free(filename_without_path);
		}*/
		res = 1;
	} else {
		MikMod_Exit();
	}
	return res;
}

static int close_file(void)
{
	wdprintf(V_DEBUG, "mikmod", "Stop!\n");
	if (module) {
		Player_Stop();
		Player_Free(module);
		MikMod_Exit();
		wdprintf(V_DEBUG, "mikmod", "Done.\n");
	} else {
		wdprintf(V_WARNING, "mikmod", "Am I playing?\n");
	}
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int mlen = VC_WriteBytes((SBYTE*)target, BUF_SIZE);
	if (!Player_Active()) mlen = 0;
	return mlen;
}

static int get_decoder_buffer_size(void)
{
	return BUF_SIZE;
}

static const char* get_file_extensions(void)
{
	return ".mod;.it;.stm;.s3m;.xm;.669;.ult;.m15";
}

static int get_current_bitrate(void)
{
	return 0;
}

static int get_length(void)
{
	return 0;
}

static int get_samplerate(void)
{
	return 44100;
}

static int get_channels(void)
{
	return 2;
}

static int get_bitrate(void)
{
	return 0;
}

static const char *get_meta_data(GmuMetaDataType gmdt, int for_current_file)
{
	char *result = NULL;
			
	switch (gmdt) {
		case GMU_META_ARTIST:
			result = "Module";
			break;
		case GMU_META_TITLE:
			result = module->songname;
			break;
		case GMU_META_ALBUM:
			break;
		case GMU_META_TRACKNR:
			break;
		case GMU_META_DATE:
			break;
		default:
			break;
	}
	return result;
}

static const char *get_file_type(void)
{
	return module->modtype;
}

static int meta_data_load(const char *filename)
{
	int result = 0;
	return result;
}

static int meta_data_close(void)
{
	return 1;
}

static GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_UTF_8;
}

static GmuDecoder gd = {
	"module_decoder",
	NULL,
	NULL,
	get_name,
	NULL,
	get_file_extensions,
	NULL,
	open_file,
	close_file,
	decode_data,
	NULL,
	get_current_bitrate,
	get_meta_data,
	NULL,
	get_samplerate,
	get_channels,
	get_length,
	get_bitrate,
	get_file_type,
	get_decoder_buffer_size,
	meta_data_load,
	meta_data_close,
	meta_data_get_charset,
	NULL,
	NULL,
	NULL
};

GmuDecoder *GMU_REGISTER_DECODER(void)
{
	return &gd;
}

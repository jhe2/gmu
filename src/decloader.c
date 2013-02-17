/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: decloader.c  Created: 081022
 *
 * Description: Shared object decoader loader for Gmu audio decoders
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "dir.h"
#include "decloader.h"
#include "gmudecoder.h"
#include "util.h"
#include "debug.h"
#if STATIC
#include "../tmp-declist.h"
#endif

union {
	void *ptr;
	GmuDecoder * (*fptr) (void);
} dlsymunion;

static const char   *dir_extensions[] = { ".so", NULL };
static DecoderChain *dc_root;
static char          extensions[1024];

static DecoderChain *dc_init_element(void)
{
	DecoderChain *dc = NULL;

	if ((dc = malloc(sizeof(DecoderChain)))) {
		dc->next = NULL;
		dc->gd = NULL;
	}
	return dc;
}

static void dc_free(DecoderChain *dc)
{
	DecoderChain *tmp = dc;

	while (tmp != NULL) {
		dc = tmp;
		tmp = tmp->next;
		if (dc) {
			if (dc->gd && dc->gd->handle) {
				wdprintf(V_DEBUG, "decloader", "Unloading decoder: %s\n", dc->gd->identifier);
				if (dc->gd->close_decoder) (*dc->gd->close_decoder)();
				dlclose(dc->gd->handle);
			}
			free(dc);
		}
	}
}

void decloader_free(void)
{
	dc_free(dc_root);
}

GmuDecoder *decloader_load_decoder(char *so_file)
{
	GmuDecoder *result = NULL;
	void       *handle;
	GmuDecoder *(*dec_load_func)(void);

	handle = dlopen(so_file, RTLD_LAZY);
	if (!handle) {
		wdprintf(V_ERROR, "decloader", "%s\n", dlerror());
		result = 0;
	} else {
		char *error;
		dlsymunion.ptr = dlsym(handle, "gmu_register_decoder");
		dec_load_func  = dlsymunion.fptr;
		error = dlerror();
		if (error) {
			wdprintf(V_ERROR, "decloader", "%s\n", error);
		} else {
			result = (*dec_load_func)();
			result->handle = handle;
		}
	}
	dlerror(); /* Clear any possibly existing error */

	return result;
}

int decloader_load_all(char *directory)
{
	Dir           dir;
	int           res = 0;
	DecoderChain *dc;

	wdprintf(V_DEBUG, "decloader", "Configuring decoder extension...\n");
	dir_set_ext_filter((const char **)&dir_extensions, 0);

	dc = dc_init_element();
	dc_root = dc;
	wdprintf(V_DEBUG, "decloader", "Searching...\n");

	dir_init(&dir);
	dir_set_base_dir(&dir, "/");
	if (dir_read(&dir, directory, 0)) {
		int i, num = dir_get_number_of_files(&dir);

		wdprintf(V_INFO, "decloader", "%d decoders found.\n", num-2);
		for (i = 0; i < num; i++) {
			GmuDecoder *gd;
			char        fpath[256];

			if (dir_get_flag(&dir, i) == REG_FILE) {
				snprintf(fpath, 255, "%s/%s", dir_get_path(&dir), dir_get_filename(&dir, i));
				if ((gd = decloader_load_decoder(fpath))) {
					wdprintf(V_INFO, "decloader", "Loading %s was successful.\n", dir_get_filename(&dir, i));
					wdprintf(V_INFO, "decloader", "%s: Name: %s\n", gd->identifier, (*gd->get_name)());
					if (gd->get_file_extensions) {
						int len = strlen(extensions);
						wdprintf(V_INFO, "decloader", "%s: File extensions: %s\n", gd->identifier, (*gd->get_file_extensions)());
						snprintf(extensions+len, 1023-len, "%s;", (*gd->get_file_extensions)());
					}
					dc->gd = gd;
					dc->next = dc_init_element();
					dc = dc->next;
					res++;
				} else {
					wdprintf(V_WARNING, "decloader", "Loading %s was unsuccessful.\n", dir_get_filename(&dir, i));
				}
			}
		}
		dir_free(&dir);
	}
	return res;
}

GmuDecoder *decloader_get_decoder_for_extension(char *file_extension)
{
	DecoderChain *dc = dc_root;
	GmuDecoder   *gd = NULL;

	if (file_extension) {
		while (dc->next) {
			char ext[512], ext2[512] = "";
			if (dc->gd->get_file_extensions) {
				strtoupper(ext, (*dc->gd->get_file_extensions)(), 511);
				strtoupper(ext2, file_extension, 511);
				if (strstr(ext, ext2) != NULL) {
					wdprintf(V_INFO, "decloader", "Matching decoder for %s: %s\n",
						     file_extension, dc->gd->identifier);
					break;
				}
			}
			dc = dc->next;
		}
		gd = dc->gd;
	}
	if (dc->gd == NULL) wdprintf(V_INFO, "decloader", "No matching decoder found for %s.\n", file_extension);
	return gd;
}

GmuDecoder *decloader_get_decoder_for_mime_type(char *mime_type)
{
	DecoderChain *dc = dc_root;
	GmuDecoder   *gd = NULL;

	if (mime_type) {
		while (dc->next) {
			if (dc->gd->get_mime_types) {
				const char *mt = (*dc->gd->get_mime_types)();
				if (strstr(mt, mime_type) != NULL) {
					wdprintf(V_INFO, "decloader", "Matching decoder for %s: %s\n",
						     mime_type, dc->gd->identifier);
					break;
				}
			}
			dc = dc->next;
		}
		gd = dc->gd;
	}
	if (dc->gd == NULL) wdprintf(V_INFO, "decloader", "No matching decoder found for %s.\n", mime_type);
	return gd;
}


GmuDecoder *decloader_get_decoder_for_data_chunk(char *data, int size)
{
	DecoderChain *dc = dc_root;
	GmuDecoder   *gd = NULL;

	if (data && size > 0) {
		while (dc->next) {
			if (dc->gd->data_check_magic_bytes) {
				if ((*dc->gd->data_check_magic_bytes)(data, size)) { /* match found */
					wdprintf(V_INFO, "decloader", "Matching decoder found: %s\n",
					         dc->gd->identifier);
					break;
				}
			} else {
				wdprintf(V_INFO, "decloader", "%s does not support magic bytes check.\n",
				         dc->gd->identifier);
			}
			dc = dc->next;
		}
		gd = dc->gd;
	}
	if (dc->gd == NULL) wdprintf(V_INFO, "decloader", "No matching decoder found.\n");
	return gd;
}

char *decloader_get_all_extensions(void)
{
	return extensions;
}

GmuDecoder *decloader_decoder_list_get_next_decoder(int getfirst)
{
	static DecoderChain *dc = NULL;
	GmuDecoder          *gd = NULL;

	dc = (getfirst ? dc_root : dc->next);
	if (dc) gd = dc->gd;
	return gd;
}

int decloader_load_builtin_decoders(void)
{
	int res = 0;
#if STATIC
	DecoderChain *dc;
	int i;

	dc = dc_init_element();
	dc_root = dc;
	for (i = 0; decload_funcs[i]; i++) {
		wdprintf(V_INFO, "decloader", "Loading internal decoder %d...\n", i);
		dc->gd = (*decload_funcs[i])();
		wdprintf(V_INFO, "decloader", "Loading decoder %d was successful.\n", i);
		wdprintf(V_INFO, "decloader", "%s: Name: %s\n", dc->gd->identifier, (*dc->gd->get_name)());
		if (dc->gd->get_file_extensions) {
			int len = strlen(extensions);
			wdprintf(V_INFO, "decloader", "%s: File extensions: %s\n", dc->gd->identifier, (*dc->gd->get_file_extensions)());
			snprintf(extensions+len, 1023-len, "%s;", (*dc->gd->get_file_extensions)());
		}
		dc->next = dc_init_element();
		dc = dc->next;
		res = 1;
	}
#endif
	return res;
}

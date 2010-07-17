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
				printf("decloader: Unloading decoder: %s\n", dc->gd->identifier);
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
		printf("decloader: %s\n", dlerror());
		result = 0;
	} else {
		char *error;
		dlsymunion.ptr = dlsym(handle, "gmu_register_decoder");
		dec_load_func  = dlsymunion.fptr;
		error = dlerror();
		if (error) {
			printf("decloader: %s\n", error);
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

	dir_set_ext_filter((const char **)&dir_extensions, 0);

	dc = dc_init_element();
	dc_root = dc;

	if (dir_read(&dir, directory, 0)) {
		int i;

		printf("decloader: %d decoders found.\n", dir_get_number_of_files(&dir));
		for (i = 0; i < dir_get_number_of_files(&dir); i++) {
			GmuDecoder *gd;
			char        fpath[256];

			printf("decloader: Loading %s was ", dir_get_filename(&dir, i));
			snprintf(fpath, 255, "%s/%s", dir_get_path(&dir), dir_get_filename(&dir, i));
			if ((gd = decloader_load_decoder(fpath))) {
				printf("successful.\n");
				printf("decloader: %s: Name: %s\n", gd->identifier, (*gd->get_name)());
				if (gd->get_file_extensions) {
					int len = strlen(extensions);
					printf("decloader: %s: File extensions: %s\n", gd->identifier, (*gd->get_file_extensions)());
					snprintf(extensions+len, 1023-len, "%s;", (*gd->get_file_extensions)());
				}
				dc->gd = gd;
				dc->next = dc_init_element();
				dc = dc->next;
				res++;
			} else {
				printf("unsuccessful.\n");
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
					printf("decloader: Matching decoder for %s: %s\n",
						   file_extension, dc->gd->identifier);
					break;
				}
			}
			dc = dc->next;
		}
		gd = dc->gd;
	}
	if (dc->gd == NULL) printf("decloader: No matching decoder found for %s.\n", file_extension);
	return gd;
}

char *decloader_get_all_extensions(void)
{
	return extensions;
}

GmuDecoder *decloader_decoder_list_get_next_decoder(int getfirst)
{
	static DecoderChain *dc = NULL;

	dc = (getfirst ? dc_root : dc->next);
	return dc->gd;
}

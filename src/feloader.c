/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: feloader.c  Created: 081228
 *
 * Description: Shared object decoader loader for Gmu frontend plugins
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
#include "feloader.h"
#include "gmufrontend.h"
#include "util.h"

union {
	void *ptr;
	GmuFrontend * (*fptr) (void);
} dlsymunion;

static const char   *dir_extensions[] = { ".so", NULL };
static FrontendChainElement *fec_root;

static FrontendChainElement *fec_init_element(void)
{
	FrontendChainElement *fec = NULL;

	if ((fec = malloc(sizeof(FrontendChainElement)))) {
		fec->next = NULL;
		fec->gf = NULL;
	}
	return fec;
}

static void fec_free(FrontendChainElement *fec)
{
	FrontendChainElement *tmp = fec;

	while (tmp != NULL) {
		fec = tmp;
		tmp = tmp->next;
		if (fec) {
			if (fec->gf && fec->gf->handle) {
				if (fec->gf->frontend_shutdown)	(*fec->gf->frontend_shutdown)();
				dlclose(fec->gf->handle);
			}
			free(fec);
		}
	}
}

void feloader_free(void)
{
	fec_free(fec_root);
}

GmuFrontend *feloader_load_frontend(char *so_file)
{
	GmuFrontend *result = NULL;
	void        *handle;
	GmuFrontend *(*fe_load_func)(void);

	handle = dlopen(so_file, RTLD_LAZY);
	if (!handle) {
		printf("feloader: %s\n", dlerror());
		result = 0;
	} else {
		char *error;
		dlsymunion.ptr = dlsym(handle, "gmu_register_frontend");
		fe_load_func   = dlsymunion.fptr;
		error = dlerror();
		if (error) {
			printf("feloader: %s\n", error);
		} else {
			result = (*fe_load_func)();
			(*result->frontend_init)(); /* Run frontend init function */
			result->handle = handle;
		}
	}
	dlerror(); /* Clear any possibly existing error */

	return result;
}

int feloader_load_all(char *directory)
{
	Dir                   dir;
	int                   res = 0;
	FrontendChainElement *fec;

	dir_set_ext_filter((const char **)&dir_extensions, 0);

	fec = fec_init_element();
	fec_root = fec;

	if (dir_read(&dir, directory, 0)) {
		int i;

		printf("feloader: %d frontends found.\n", dir_get_number_of_files(&dir));
		for (i = 0; i < dir_get_number_of_files(&dir); i++) {
			GmuFrontend *gf;
			char         fpath[256];

			printf("feloader: Loading %s was ", dir_get_filename(&dir, i));
			snprintf(fpath, 255, "%s/%s", dir_get_path(&dir), dir_get_filename(&dir, i));
			if ((gf = feloader_load_frontend(fpath))) {
				printf("successful.\n");
				printf("feloader: %s: Name: %s\n", gf->identifier, (*gf->get_name)());
				fec->gf = gf;
				fec->next = fec_init_element();
				fec = fec->next;
				res++;
			} else {
				printf("unsuccessful.\n");
			}
		}
		dir_free(&dir);
	}
	return res;
}

GmuFrontend *feloader_frontend_list_get_next_frontend(int getfirst)
{
	static FrontendChainElement *fec = NULL;

	fec = (getfirst ? fec_root : fec->next);
	return fec->gf;
}

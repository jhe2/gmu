/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
#include "debug.h"
#if STATIC
#include "../tmp-felist.h"
#endif

union {
	void *ptr;
	GmuFrontend * (*fptr) (void);
} dlsymunion;

static const char           *dir_extensions[] = { ".so", NULL };
static FrontendChainElement *fec_root = NULL;

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
			if (fec->gf) feloader_unload_frontend(fec->gf);
			free(fec);
		}
	}
}

void feloader_free(void)
{
	fec_free(fec_root);
}

/* Unloads a frontend; Returns 1 on success, 0 otherwise */
int feloader_unload_frontend(GmuFrontend *gf)
{
	int res = 0;
	if (gf && gf->handle) {
		if (gf->frontend_shutdown) (*gf->frontend_shutdown)();
		dlclose(gf->handle);
		res = 1;
	}
	return res;
}

/* Loads a frontend and returns a frontend object */
GmuFrontend *feloader_load_frontend(char *so_file)
{
	GmuFrontend *result = NULL;
	void        *handle;
	GmuFrontend *(*fe_load_func)(void);

	handle = dlopen(so_file, RTLD_LAZY);
	if (!handle) {
		wdprintf(V_ERROR, "feloader", "%s\n", dlerror());
		result = 0;
	} else {
		char *error;
		dlsymunion.ptr = dlsym(handle, "gmu_register_frontend");
		fe_load_func   = dlsymunion.fptr;
		error = dlerror();
		if (error) {
			wdprintf(V_ERROR, "feloader", "%s\n", error);
		} else {
			result = (*fe_load_func)();
			if ((*result->frontend_init)()) { /* Run frontend init function */
				result->handle = handle;
			} else { /* Frontend init returned with error */
				dlclose(handle);
				result = NULL;
			}
		}
	}
	dlerror(); /* Clear any possibly existing error */

	return result;
}

/* Loads a frontend and adds it to the frontend chain */
int feloader_load_single_frontend(char *so_file)
{
	FrontendChainElement *fec;
	GmuFrontend          *gf;
	int                   res = 0;

	if (!fec_root) fec_root = fec_init_element();
	fec = fec_root;
	while (fec && fec->next) {
		fec = fec->next;
	}

	if ((gf = feloader_load_frontend(so_file))) {
		wdprintf(V_INFO, "feloader", "Loading %s was successful.\n", so_file);
		wdprintf(V_INFO, "feloader", "%s: Name: %s\n", gf->identifier, (*gf->get_name)());
		fec->gf = gf;
		fec->next = fec_init_element();
		res = 1;
	} else {
		wdprintf(V_WARNING, "feloader", "Loading %s was unsuccessful.\n", so_file);
	}
	return res;
}

int feloader_load_all(char *directory)
{
	Dir                   dir;
	int                   res = 0;
	FrontendChainElement *fec;

	dir_set_ext_filter((const char **)&dir_extensions, 0);

	fec = fec_init_element();
	fec_root = fec;

	dir_init(&dir);
	dir_set_base_dir(&dir, "/");
	if (dir_read(&dir, directory, 0)) {
		int i, num = dir_get_number_of_files(&dir);

		wdprintf(V_INFO, "feloader", "%d frontends found.\n", num-2);
		for (i = 0; i < num; i++) {
			GmuFrontend *gf;
			char         fpath[256];

			if (dir_get_flag(&dir, i) == REG_FILE) {
				snprintf(fpath, 255, "%s/%s", dir_get_path(&dir), dir_get_filename(&dir, i));
				if ((gf = feloader_load_frontend(fpath))) {
					wdprintf(V_INFO, "feloader", "feloader: Loading %s was successful.\n", dir_get_filename(&dir, i));
					wdprintf(V_INFO, "feloader", "%s: Name: %s\n", gf->identifier, (*gf->get_name)());
					fec->gf = gf;
					fec->next = fec_init_element();
					fec = fec->next;
					res++;
				} else {
					wdprintf(V_WARNING, "feloader", "feloader: Loading %s was unsuccessful.\n", dir_get_filename(&dir, i));
				}
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

int feloader_load_builtin_frontends(void)
{
	int res = 0;
#if STATIC
	int i;
	FrontendChainElement *fec;
	
	fec = fec_init_element();
	fec_root = fec;
	for (i = 0; feload_funcs[i]; i++) {
		wdprintf(V_INFO, "feloader", "Loading internal frontend %d...\n", i);
		fec->gf = (*feload_funcs[i])();
		(*fec->gf->frontend_init)();
		fec->next = fec_init_element();
		fec = fec->next;
		res = 1;
	}
#endif
	return res;
}

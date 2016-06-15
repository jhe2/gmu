/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
 *
 * File: dirparser.c  Created: 130702
 *
 * Description: Directory parser
 */

#include <stdlib.h>
#include "debug.h"
#include "util.h"
#include "dir.h"
#include "dirparser.h"
#include "core.h"

int dirparser_walk_through_directory_tree(const char *directory, int (fn(void *arg, const char *filename)), void *arg, int dir_depth)
{
	Dir  *dir;
	int   i;
	char  filetype[16];
	int   result = 1;

	wdprintf(V_INFO, "dirparser", "Parsing '%s'...\n", directory);

	dir = dir_init();
	dir_set_base_dir(dir, "/");
	dir_set_ext_filter(dir, gmu_core_get_file_extensions(), 1);
	if (dir_read(dir, directory, 1)) {
		for (i = 0; i < dir_get_number_of_files(dir); i++) {
			if (dir_get_flag(dir, i) == DIRECTORY) {
				if (dir_get_filename(dir, i)[0] != '.') {
					char *f = dir_get_filename_with_full_path_alloc(dir, i);
					if (f) {
						if (dir_depth < DIRPARSER_MAX_DEPTH) {
							dirparser_walk_through_directory_tree(f, fn, arg, dir_depth + 1);
						} else {
							wdprintf(
								V_WARNING,
								"dirparser",
								"Maximum directory depth of %d exceeded for directory: %s\n",
								DIRPARSER_MAX_DEPTH,
								f);
						}
						free(f);
					}
				}
			} else {
				char       *filename_tmp = dir_get_filename(dir, i);
				const char *tmp = filename_tmp ? get_file_extension(filename_tmp) : NULL;
				char       *f   = dir_get_filename_with_full_path_alloc(dir, i);
				filetype[0] = '\0';
				if (tmp != NULL) strtoupper(filetype, tmp, 15);
				if (f) {
					(*fn)(arg, f);
					free(f);
				}
				/*wdprintf(V_DEBUG, "dirparser", "[%4d] %s\n", i, dir_get_filename(dir, i));*/
			}
		}
	}
	dir_free(dir);

	wdprintf(V_INFO, "dirparser", "Done parsing %s.\n", directory);
	return result;
}

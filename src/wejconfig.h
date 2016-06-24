/*
 * wej's config file parser
 *
 * File: wejconfig.h
 *
 * Copyright (c) 2003-2015 Johannes Heimansberg
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef WEJCONFIG_H
#define WEJCONFIG_H
#define MAXKEYS 128
#define CFG_OUT_OF_MEMORY -2
#define CFG_ERROR -1
#define CFG_SUCCESS 0

typedef struct
{
	char  *key[MAXKEYS];
	char  *value[MAXKEYS];
	char **presets[MAXKEYS];
	size_t lastkey;
	char  *file;
} ConfigFile;

ConfigFile *cfg_init(void);
void        cfg_free(ConfigFile *cf);
int         cfg_add_key(ConfigFile *cf, const char *key, const char *value);
int         cfg_key_add_presets(ConfigFile *cf, const char *key, ...);
int         cfg_read_config_file(ConfigFile *cf, const char *filename);
int         cfg_set_output_config_file(ConfigFile *cf, const char *filename);
int         cfg_write_config_file(ConfigFile *cf, const char *filename);
char       *cfg_get_key_value(ConfigFile *cf, const char *key);
char       *cfg_get_key_value_ignore_case(ConfigFile *cf, const char *key);
int         cfg_get_boolean_value(ConfigFile *cf, const char *key);
int         cfg_get_int_value(ConfigFile *cf, const char *key);
int         cfg_compare_value(ConfigFile *cf, const char *key, const char *cmp_val, int ignore_case);
int         cfg_check_config_file(const char *filename);
char       *cfg_get_path_to_config_file(const char *filename);
int         cfg_is_key_available(ConfigFile *cf, const char *key);
int         cfg_add_key_if_not_present(ConfigFile *cf, const char *key, const char *value);
char      **cfg_key_get_presets(ConfigFile *cf, const char *key);
char        *cfg_get_key(ConfigFile *cf, size_t n);
#endif

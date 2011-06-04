/*
 * wejp's Config File Parser
 *
 * File: wejpconfig.h
 *
 * Copyright (c) 2003-2011 Johannes Heimansberg
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

#ifndef _WEJPCONFIG_H
#define _WEJPCONFIG_H
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define MAXKEYS 128
#define CFG_OUT_OF_MEMORY -2
#define CFG_ERROR -1
#define CFG_SUCCESS 0

typedef struct
{
	char *key[MAXKEYS];
	char *value[MAXKEYS];
	int  lastkey;
} ConfigFile;

void  cfg_init_config_file_struct(ConfigFile *cf);
int   cfg_add_key(ConfigFile *cf, char *key, char *value);
void  cfg_free_config_file_struct(ConfigFile *cf);
int   cfg_read_config_file(ConfigFile *cf, char *filename);
int   cfg_write_config_file(ConfigFile *cf, char *filename);
char *cfg_get_key_value(ConfigFile cf, char *key);
char *cfg_get_key_value_ignore_case(ConfigFile cf, char *key);
int   cfg_check_config_file(char *filename);
char *cfg_get_path_to_config_file(char *filename);
int   cfg_is_key_available(ConfigFile cf, char *key);
int   cfg_add_key_if_not_present(ConfigFile *cf, char *key, char *value);
#endif

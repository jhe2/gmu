/*
 * wejp's Config File Parser
 *
 * File: wejpconfig.c
 *
 * Copyright (c) 2003-2010 Johannes Heimansberg
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

#define VERSION 20101110
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wejpconfig.h"
#define MAX_LINE_LENGTH 256

/* Returns the complete path to ~/"filename" */
char *cfg_get_path_to_config_file(char *filename)
{
	char *home_directory, *path_to_config_file;

	home_directory = getenv("HOME");
	path_to_config_file = (char*)malloc((strlen(home_directory) + 
	                                     strlen(filename) + 2) * sizeof(char));
	
	if (path_to_config_file)
		sprintf(path_to_config_file, "%s/%s", home_directory, filename);
	return path_to_config_file;
}

/* Must be called before the first use of a ConfigFile struct element */
void cfg_init_config_file_struct(ConfigFile *cf)
{
	cf->lastkey = 0;
}

/* Checks wether the config file exists or not */
int cfg_check_config_file(char *filename)
{
	FILE *file;
	int  result = CFG_SUCCESS;
	file = fopen(filename, "r");
	if (file == NULL)
		result = CFG_ERROR;
	else
		fclose(file);
	return result;
}

/* Add a new key to the configuration */
int cfg_add_key(ConfigFile *cf, char *key, char *value)
{
	int result = CFG_SUCCESS;
	int strsize = 0;
	int i;
	
	if (cfg_get_key_value(*cf, key) != NULL) { /* Key already exists->overwrite */
		for (i = 0; i < cf->lastkey; i++)
			if (strncmp(key, cf->key[i], MAX_LINE_LENGTH-1) == 0) {
				free(cf->key[i]); /* Free allocated memory */
				free(cf->value[i]);

				/* Allocate memory for the new string and save it... */
				strsize = (strlen(key) < MAX_LINE_LENGTH-1 ? strlen(key) : MAX_LINE_LENGTH-2) + 1;
				cf->key[i] = (char*)malloc(strsize * sizeof(char));
				if (cf->key[i]) {
					snprintf(cf->key[i], strsize, "%s", key);
					strsize = (strlen(value) < MAX_LINE_LENGTH-1 ? strlen(value) : MAX_LINE_LENGTH-2) + 1;
					cf->value[i] = (char*)malloc(strsize * sizeof(char));
					if (cf->value[i]) {
						snprintf(cf->value[i], strsize, "%s", value);
					} else {
						result = CFG_OUT_OF_MEMORY;
					}
				} else {
					result = CFG_OUT_OF_MEMORY;
				}
				break;
			}
	} else if (cf->lastkey < MAXKEYS) {
		strsize = (strlen(key) < MAX_LINE_LENGTH-1 ? strlen(key) : MAX_LINE_LENGTH-2) + 1;
		cf->key[cf->lastkey] = (char*)malloc(strsize * sizeof(char));
		if (cf->key[cf->lastkey]) {
			sprintf(cf->key[cf->lastkey], "%s", key);

			strsize = (strlen(key) < MAX_LINE_LENGTH-1 ? strlen(value) : MAX_LINE_LENGTH-2) + 1;
			cf->value[cf->lastkey] = (char*)malloc(strsize * sizeof(char));
			if (cf->value[cf->lastkey]) {
				sprintf(cf->value[cf->lastkey], "%s", value);
				(cf->lastkey)++;
			} else {
				result = CFG_OUT_OF_MEMORY;
			}
		} else {
			result = CFG_OUT_OF_MEMORY;
		}
	} else {
		result = CFG_ERROR;
	}
	return result;
}

/* Frees all memory allocated by read_config_file() */
void cfg_free_config_file_struct(ConfigFile *cf)
{
	int i;
	for (i = 0; i < cf->lastkey; i++) {
		if (cf->key[i]) free(cf->key[i]);
		if (cf->value[i]) free(cf->value[i]);
	}
	cf->lastkey = -1;
}

/* Loads a config file from disk */
int cfg_read_config_file(ConfigFile *cf, char *filename)
{
	int   result = CFG_SUCCESS;
	FILE *file;
	char  ch;
	int   bufcnt;
	char  key_buffer[MAX_LINE_LENGTH] = "", value_buffer[MAX_LINE_LENGTH] = "";

	/* parse config file and read keys+key values */
	file = fopen(filename, "r");
	if (file) {
		while(!feof(file)) {
			ch = fgetc(file);
			/* Skip blanks... */
			if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
				while (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ch = fgetc(file);
			/* ...and comments (#)... */
			do {
				if (ch == '#') {
					while (ch != '\n' && ch != '\r') ch = fgetc(file);
					ch = fgetc(file);
				}
				if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
					ch = fgetc(file);
			} while (ch == '#' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

			bufcnt = 0;
			/* Read key name: */
			while (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && 
			       ch != '=' && !feof(file) && bufcnt < MAX_LINE_LENGTH-2) {
				key_buffer[bufcnt] = ch;
				bufcnt++;
				ch = fgetc(file);
			}
			key_buffer[bufcnt] = '\0';

			while (ch != '=' && !feof(file)) {
				ch = fgetc(file);
			}
			ch = fgetc(file);

			/* Skip blanks... */
			if (ch == ' ' || ch == '\t')
				while (ch == ' ' || ch == '\t') ch = fgetc(file);

			/* Read key value: */
			bufcnt = 0;
			while (ch != '\n' && ch != '\r' && !feof(file) && 
			       !feof(file) && bufcnt < MAX_LINE_LENGTH-2) {
				value_buffer[bufcnt] = ch;
				bufcnt++;
				ch = fgetc(file);
			}
			value_buffer[bufcnt] = '\0';

			if (strlen(key_buffer) > 0)
				result = cfg_add_key(cf, key_buffer, value_buffer);
		}
		fclose(file);
	} else {
		/*printf("config: Cannot open config file: %s\n", filename);*/
		result = CFG_ERROR;
	}
	return result;
}

/* Saves the configuration to file */
int cfg_write_config_file(ConfigFile *cf, char *filename)
{
	FILE *file;
	int   i = 0, result = CFG_SUCCESS;
	char  buffer[MAX_LINE_LENGTH];

	file = fopen(filename, "w");
	if (file) {
		while (i < cf->lastkey) {
			snprintf(buffer, MAX_LINE_LENGTH, "%s=%s\n", cf->key[i], cf->value[i]);
			if (!fwrite(buffer, strlen(buffer) * sizeof(char), 1, file)) {
				result = CFG_ERROR;
				printf("config: ERROR: Failed writing configuration file.\n");
				break;
			}
			i++;
		}
		fclose(file);
	} else {
		printf("config: Failed opening %s for write access.\n", filename);
		result = CFG_ERROR;
	}
	return result;
}

/* Returns the value (as string) of "key" */
char *cfg_get_key_value(ConfigFile cf, char *key)
{
	char *result = NULL;
	int   i;
	
	for (i = 0; i < cf.lastkey; i++)
		if (strncmp(key, cf.key[i], MAX_LINE_LENGTH-1) == 0) {
			result = cf.value[i];
			break;
		}
	return result;
}

char *cfg_get_key_value_ignore_case(ConfigFile cf, char *key)
{
	char *result = NULL;
	int   i;
	
	for (i = 0; i < cf.lastkey; i++)
		if (strncasecmp(key, cf.key[i], MAX_LINE_LENGTH-1) == 0) {
			result = cf.value[i];
			break;
		}
	return result;
}

int cfg_is_key_available(ConfigFile cf, char *key)
{
	int result = FALSE;
	int i;

	for (i = 0; i < cf.lastkey; i++)
		if (strncmp(key, cf.key[i], MAX_LINE_LENGTH-1) == 0) {
			result = TRUE;
			break;
		}
	return result;	
}

int cfg_add_key_if_not_present(ConfigFile *cf, char *key, char *value)
{
	int success = 0;

	if (!cfg_is_key_available(*cf, key)) {
		success = (cfg_add_key(cf, key, value) == 0 ? 1 : 0);
	}
	return success;
}

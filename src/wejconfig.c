/*
 * wej's config file parser
 *
 * File: wejconfig.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "wejconfig.h"
#define MAX_LINE_LENGTH 256

/**
 * Returns the complete path to ~/"filename".
 */
char *cfg_get_path_to_config_file(const char *filename)
{
	char *home_directory, *path_to_config_file = NULL;

	home_directory = getenv("HOME");
	if (home_directory)
		path_to_config_file = (char*)malloc(
			(strlen(home_directory) + strlen(filename) + 2) * sizeof(char)
		);
	
	if (path_to_config_file)
		sprintf(path_to_config_file, "%s/%s", home_directory, filename);
	return path_to_config_file;
}

/**
 * Initializes an empty ConfigFile object.
 */
ConfigFile *cfg_init(void)
{
	return (ConfigFile *)calloc(1, sizeof(ConfigFile));
}

/**
 * Frees an existing ConfigFile object.
 */
void cfg_free(ConfigFile *cf)
{
	if (cf) {
		size_t i;
		for (i = 0; i < cf->lastkey; i++) {
			if (cf->key[i]) free(cf->key[i]);
			if (cf->value[i]) free(cf->value[i]);
			if (cf->presets[i]) {
				size_t j;
				for (j = 0; cf->presets[i][j]; j++) {
					free(cf->presets[i][j]);
				}
				free(cf->presets[i]);
			}
		}
		cf->lastkey = 0;
		free(cf->file);
		free(cf);
		cf = NULL;
	}
}

/**
 * Checks wether the config file exists or not.
 */
int cfg_check_config_file(const char *filename)
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

/**
 * Internal helper function to initialize a key/value pair at position
 * "pos" in the key/value arrays. Allocates memory as needed. Does not
 * free memory in case the key already exists.
 */
static int set_key_at_position(ConfigFile *cf, size_t pos, const char *key, const char *value)
{
	int    result = CFG_SUCCESS;
	size_t strsize = strlen(key) + 1;

	if (strsize > MAX_LINE_LENGTH) strsize = MAX_LINE_LENGTH;
	cf->key[pos] = (char*)malloc(strsize * sizeof(char));
	if (cf->key[pos]) {
		snprintf(cf->key[pos], strsize, "%s", key);
		strsize = strlen(value) + 1;
		if (strsize > MAX_LINE_LENGTH) strsize = MAX_LINE_LENGTH;
		cf->value[pos] = (char*)malloc(strsize * sizeof(char));
		if (cf->value[pos]) {
			snprintf(cf->value[pos], strsize, "%s", value);
		} else {
			result = CFG_OUT_OF_MEMORY;
		}
	} else {
		result = CFG_OUT_OF_MEMORY;
	}
	return result;
}

/**
 * Adds a new key to the configuration or overrides an existing one.
 */
int cfg_add_key(ConfigFile *cf, const char *key, const char *value)
{
	int    result = CFG_SUCCESS;
	size_t i;

	if (cfg_get_key_value(cf, key) != NULL) { /* Key already exists->overwrite */
		for (i = 0; i < cf->lastkey; i++)
			if (strncmp(key, cf->key[i], MAX_LINE_LENGTH-1) == 0) {
				free(cf->key[i]); /* Free allocated memory */
				free(cf->value[i]);
				result = set_key_at_position(cf, i, key, value);
				break;
			}
	} else if (cf->lastkey < MAXKEYS) {
		result = set_key_at_position(cf, cf->lastkey, key, value);
		if (result == CFG_SUCCESS) (cf->lastkey)++;
	} else {
		result = CFG_ERROR;
	}
	return result;
}

/**
 * Loads a config file from disk.
 */
int cfg_read_config_file(ConfigFile *cf, const char *filename)
{
	int   result = CFG_SUCCESS;
	FILE *file;
	char  ch;
	int   bufcnt;
	char  key_buffer[MAX_LINE_LENGTH] = "", value_buffer[MAX_LINE_LENGTH] = "";

	/* parse config file and read keys+key values */
	file = fopen(filename, "r");
	if (file) {
		size_t lf = strlen(filename);
		if (lf > 0) cf->file = malloc(lf+1);
		if (cf->file) {
			memcpy(cf->file, filename, lf+1);
		}
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
		result = CFG_ERROR;
	}
	return result;
}

/**
 * Sets the output config file, so that cfg_write_config_file() can
 * be called with NULL as second argument, even if the configuration
 * has not been loaded from file or if the configuration is supposed to
 * be written to a different file than the one it has been loaded from.
 */
int cfg_set_output_config_file(ConfigFile *cf, const char *filename)
{
	int res = CFG_ERROR;
	size_t lf = strlen(filename);
	if (cf->file) free(cf->file);
	if (lf > 0) cf->file = malloc(lf+1);
	if (cf->file) {
		memcpy(cf->file, filename, lf+1);
		res = CFG_SUCCESS;
	}
	return res;
}

/**
 * Saves the configuration to file.
 * If 'filename' is NULL, the function tries to save to the same file
 * that was used to load the configuration. Obviously, this won't work
 * if the configuration has not been loaded from file.
 */
int cfg_write_config_file(ConfigFile *cf, const char *filename)
{
	FILE *file = NULL;
	int   result = CFG_SUCCESS;
	char  buffer[MAX_LINE_LENGTH];

	if (filename || cf->file)
		file = fopen(filename ? filename : cf->file, "w");
	if (file) {
		size_t i;
		for (i = 0; i < cf->lastkey; i++) {
			snprintf(buffer, MAX_LINE_LENGTH, "%s=%s\n", cf->key[i], cf->value[i]);
			if (!fwrite(buffer, strlen(buffer) * sizeof(char), 1, file)) {
				result = CFG_ERROR;
				break;
			}
		}
		fclose(file);
	} else {
		result = CFG_ERROR;
	}
	return result;
}

/**
 * Returns the value (as string) of "key"
 */
char *cfg_get_key_value(ConfigFile *cf, const char *key)
{
	char  *result = NULL;
	size_t i;

	for (i = 0; i < cf->lastkey; i++)
		if (strncmp(key, cf->key[i], MAX_LINE_LENGTH-1) == 0) {
			result = cf->value[i];
			break;
		}
	return result;
}

char *cfg_get_key_value_ignore_case(ConfigFile *cf, const char *key)
{
	char  *result = NULL;
	size_t i;

	for (i = 0; i < cf->lastkey; i++)
		if (strncasecmp(key, cf->key[i], MAX_LINE_LENGTH-1) == 0) {
			result = cf->value[i];
			break;
		}
	return result;
}

static const char *cfg_boolean_true[] = {
	"yes",
	"1",
	"true",
	NULL
};

/**
 * Interprets the value of the given key as boolean and returns either
 * 1 for true or 0 for false. All possible values for true are defined
 * in cfg_boolean_true (see above). Everything else is false.
 */
int cfg_get_boolean_value(ConfigFile *cf, const char *key)
{
	int         res = 0;
	int         i;
	const char *v = cfg_get_key_value(cf, key);
	if (v) {
		for (i = 0; cfg_boolean_true[i]; i++) {
			if (strcasecmp(v, cfg_boolean_true[i]) == 0) {
				res = 1;
				break;
			}
		}
	}
	return res;
}

/**
 * Interprets the value of the given key as an integer. Every non-number
 * value will be interpreted as 0.
 */
int cfg_get_int_value(ConfigFile *cf, const char *key)
{
	int         res = 0;
	const char *v = cfg_get_key_value(cf, key);
	if (v) res = atoi(v);
	return res;
}

/**
 * Compares the value of the given key with the supplied string and
 * returns 1 if it matches or 0 otherwise. If ignore_case is true, the
 * letter's case will be ignored when comparing. If the key does not
 * exist, the result will always be false.
 */
int cfg_compare_value(ConfigFile *cf, const char *key, const char *cmp_val, int ignore_case)
{
	int res = 0;
	if (key && cmp_val) {
		const char *val = cfg_get_key_value(cf, key);
		if (val) {
			if (!ignore_case && strcmp(val, cmp_val) == 0)
				res = 1;
			else if (ignore_case && strcasecmp(val, cmp_val) == 0)
				res = 1;
		}
	}
	return res;
}

/**
 * If the supplied key is not found, -1 is returned
 */
static int int_cfg_get_key_id(ConfigFile *cf, const char *key)
{
	int    result = -1;
	size_t i;

	for (i = 0; i < cf->lastkey; i++)
		if (strncmp(key, cf->key[i], MAX_LINE_LENGTH-1) == 0) {
			result = i;
			break;
		}
	return result;
}

int cfg_is_key_available(ConfigFile *cf, const char *key)
{
	return (int_cfg_get_key_id(cf, key) >= 0) ? 1 : 0;
}

int cfg_add_key_if_not_present(ConfigFile *cf, const char *key, const char *value)
{
	int success = 0;

	if (!cfg_is_key_available(cf, key)) {
		success = (cfg_add_key(cf, key, value) == 0 ? 1 : 0);
	}
	return success;
}

/**
 * Adds presets to an existing key, which can be used in an UI to allow
 * the user to select a value for the key from a list of predefined
 * values. The list of preset values must end with the NULL value.
 * Returns 1 on success, 0 otherwise.
 */
int cfg_key_add_presets(ConfigFile *cf, const char *key, ...)
{
	int     res = 0;
	size_t  arg_count = 0;
	va_list args;
	char   *arg;

	if (cfg_is_key_available(cf, key)) {
		size_t i, j;
		for (i = 0; i < cf->lastkey; i++) {
			if (strncmp(key, cf->key[i], MAX_LINE_LENGTH-1) == 0)
				break;
		}

		va_start(args, key);
		for (arg_count = 0, arg = va_arg(args, char *); arg; arg = va_arg(args, char *), arg_count++);
		va_end(args);
		if (arg_count > 0) {
			cf->presets[i] = malloc((arg_count + 1) * sizeof(char *));
			if (cf->presets[i]) {
				cf->presets[i][arg_count] = NULL;
				va_start(args, key);
				for (j = 0, arg = va_arg(args, char *); arg; arg = va_arg(args, char *), j++) {
					int size = strlen(arg);
					if (size > 0) {
						/* Add preset value to key's preset list... */
						cf->presets[i][j] = malloc(size+1);
						if (cf->presets[i][j]) {
							strncpy(cf->presets[i][j], arg, size+1);
						}
					}
				}
				va_end(args);
			}
		}
		res = 1;
	}
	return res;
}

char **cfg_key_get_presets(ConfigFile *cf, const char *key)
{
	int id = int_cfg_get_key_id(cf, key);
	return cf->presets[id];
}

char *cfg_get_key(ConfigFile *cf, size_t n)
{
	char *res = NULL;
	if (n < cf->lastkey)
		res = cf->key[n];
	return res;
}

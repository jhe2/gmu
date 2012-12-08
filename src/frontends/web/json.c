/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: json.c  Created: 120811
 *
 * Description: A simple JSON parser and helper functions
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
#include "json.h"
#include "debug.h"

/* Find first non-whitespace character and return the offset.
 * Negative results denote an error. */
static int find_first_non_whitespace_char(char *str)
{
	int res = -2;
	if (str) {
		for (res = 0; str[res] == ' '  ||
		              str[res] == '\t' ||
		              str[res] == '\n' ||
		              str[res] == '\r'; res++)
			if (str[res] == '\0') {
				res = -1;
				break;
			}
	}
	return res;
}

typedef enum State { STATE_SEARCH_OBJECT, STATE_SEARCH_KEY, STATE_SEARCH_VALUE, STATE_DONE } State;

JSON_Object *json_object_new(JSON_Object *parent)
{
	JSON_Object *jo = malloc(sizeof(JSON_Object));
	if (jo) {
		jo->first_key = NULL;
		jo->parent = parent;
		jo->next = NULL;
		jo->length = 0;
		jo->parse_error = 0;
	}
	return jo;
}

void json_keys_free(JSON_Key *first_key)
{
	JSON_Key *k = first_key;
	while (k) {
		JSON_Key *tk = k;
		k = k->next;
		free(tk->key_name);
		free(tk->key_value_str);
		if (tk->key_value_object) json_object_free(tk->key_value_object);
		free(tk);
	}
}

void json_object_free(JSON_Object *object)
{
	/* Free everything attached to the object... */
	if (object) {
		if (object->first_key) json_keys_free(object->first_key);
		free(object);
	}
}

/* Attaches a key/value object to a JSON_Object */
void json_object_attach_key(JSON_Object *object, JSON_Key *key)
{
	JSON_Key *pk, *tk = pk = object->first_key;

	if (!object->first_key) {
		wdprintf(V_DEBUG, "json", "Adding first key to object...\n");
		object->first_key = key;
	} else {
		wdprintf(V_DEBUG, "json", "Adding additional key to object...\n");
		while (tk) {
			pk = tk;
			tk = tk->next;
		}
		if (pk) pk->next = key;
	}
}

int json_object_has_parse_error(JSON_Object *jo)
{
	return jo->parse_error;
}

JSON_Key *json_key_new(void)
{
	JSON_Key *jk = malloc(sizeof(JSON_Key));
	if (jk) {
		jk->key_name = NULL;
		jk->key_value_str = NULL;
		jk->key_value_number = 0.0;
		jk->key_value_boolean = 0;
		jk->key_value_array = NULL;
		jk->key_value_object = NULL;
		jk->type = EMPTY;
		jk->next = NULL;
	}
	return jk;
}

JSON_Object *json_parse_alloc(char *json_data)
{
	JSON_Object *json = NULL;
	State s = STATE_SEARCH_OBJECT;
	json = json_object_new(NULL);
	if (json_data && json) {
		JSON_Key *current_key = NULL;
		int len = strlen(json_data);
		int i;
		for (i = 0; i < len && s != STATE_DONE; i++) {
			switch (s) {
				case STATE_SEARCH_OBJECT: {
					int v = find_first_non_whitespace_char(json_data+i);
					if (v >= 0) i += v; else break;
					if (json_data[i] == '{') { /* Object found */
						s = STATE_SEARCH_KEY;
						wdprintf(V_DEBUG, "json", "JSON object found.\n");
					} else { /* Malformed JSON data */
						wdprintf(V_WARNING, "json", "ERROR: Malformed JSON data found: '{' expected.\n");
						json->parse_error = 1;
						break;
					}
					break;
				}
				case STATE_SEARCH_KEY: {
					int v = find_first_non_whitespace_char(json_data+i);
					if (v >= 0) i += v; else break;
					if (json_data[i] == '"') { /* Key beginning found */
						int start = ++i;
						char *key_name = NULL;
						wdprintf(V_DEBUG, "json", "Key found!\n");
						while (json_data[i] != '"' && i < len) i++;
						wdprintf(V_DEBUG, "json", "Key length = %d\n", i - start);
						if (i - start > 0) {
							key_name = malloc(i - start + 1);
							if (key_name) {
								int j;
								for (j = start; j < i; j++) key_name[j - start] = json_data[j];
								key_name[j - start] = '\0';
								wdprintf(V_DEBUG, "json", "Key name = [%s]\n", key_name);
								current_key = json_key_new();
								if (current_key) {
									current_key->key_name = key_name;
									json_object_attach_key(json, current_key);
								}
								s = STATE_SEARCH_VALUE;
							}
						}
					} else {
						wdprintf(V_WARNING, "json", "ERROR: Malformed JSON data found: \" expected, '%c' found instead.\n", json_data[i]);
						json->parse_error = 1;
						break;
					}
					break;
				}
				case STATE_SEARCH_VALUE: {
					wdprintf(V_DEBUG, "json", "Searching for value...\n");
					int v = find_first_non_whitespace_char(json_data+i);
					if (v >= 0) i += v; else break;
					if (json_data[i] == ':') {
						++i;
						v = find_first_non_whitespace_char(json_data+i);
						if (v >= 0) i += v; else break;
						wdprintf(V_DEBUG, "json", "Value found!\n");
						/* extract value data ... */
						/* recursion for object values; everything else will be extracted in place */
						switch (json_data[i]) {
							case '{': { /* Object */
								JSON_Object *obj;
								wdprintf(V_DEBUG, "json", "--> Object found.\n");
								obj = json_parse_alloc(json_data+i);
								i += obj->length;
								if (current_key) {
									current_key->type = OBJECT;
									current_key->key_value_object = obj;
								}
								wdprintf(V_DEBUG, "json", "<--\n");
								break;
							}
							case '"': { /* String */
								int   start = ++i;
								char *key_value = NULL;
								wdprintf(V_DEBUG, "json", "String found.\n");
								while (json_data[i] != '"' && i < len) {
									if (json_data[i] == '\\' && i-1 < len) i++;
									i++;
								}
								wdprintf(V_DEBUG, "json", "String length = %d\n", i - start);
								if (i - start >= 0) {
									key_value = malloc(i - start + 1);
									if (key_value) {
										int j, k;
										for (j = start, k = 0; j < i; j++, k++) {
											if (json_data[j] == '\\' && j < i) j++;
											key_value[k] = json_data[j];
										}
										key_value[k] = '\0';
										wdprintf(V_DEBUG, "json", "Key value = [%s]\n", key_value);
										if (current_key) {
											wdprintf(V_DEBUG, "json", "Storing value..\n");
											current_key->type = STRING;
											current_key->key_value_str = key_value;
										}
										s = STATE_SEARCH_VALUE;
									}
									i++; /* Skip closing " character */
								}
								break;
							}
							case '[': { /* Array */
								wdprintf(V_DEBUG, "json", "Array detected. Ignoring.\n");
								for (; json_data[i] != ']' && i < len; i++);
								if (json_data[i] != ']') {
									wdprintf(V_WARNING, "json", "ERROR: Invalid JSON data. No end of array found.\n");
									json->parse_error = 1;
								} else {
									i++;
								}
								break;
							}
							default: { /* Number/Boolean */
								int   start = i;
								char *key_value = NULL;
								wdprintf(V_DEBUG, "json", "Number or boolean value found (probably).\n");
								while (json_data[i] != ' ' && json_data[i] != ',' && json_data[i] != '}' && i < len) i++;
								wdprintf(V_DEBUG, "json", "String(number) length = %d\n", i - start);
								if (i - start > 0) {
									key_value = malloc(i - start + 1);
									if (key_value) {
										int j;
										for (j = start; j < i; j++) key_value[j - start] = json_data[j];
										key_value[j - start] = '\0';
										wdprintf(V_DEBUG, "json", "Key value = [%s]\n", key_value);
										if (current_key) {
											if (key_value[0] == 't' || key_value[0] == 'f') {
												current_key->type = BOOLEAN;
												current_key->key_value_boolean = key_value[0] == 't' ? 1 : 0;
												free(key_value);
											} else {
												current_key->type = NUMBER;
												current_key->key_value_number = atof(key_value);
												free(key_value);
											}
										}
										s = STATE_SEARCH_VALUE;
									}
								}
								break;
							}
						}
						/* Search for comma (,) or curly closing bracket (}) -> end of object */
						v = find_first_non_whitespace_char(json_data+i);
						if (v >= 0) i += v; else break;
						switch (json_data[i]) {
							case ',': /* End of key/value pair */
								wdprintf(V_DEBUG, "json", "Comma found -> Searching for next key.\n");
								s = STATE_SEARCH_KEY;
								break;
							case '}': /* End of object */
								wdprintf(V_DEBUG, "json", "End of object detected.\n");
								i++;
								s = STATE_DONE;
								json->length = i;
								break;
							default:
								wdprintf(V_WARNING, "json", "ERROR: Unexpected char found: '%c'\n", json_data[i]);
								json->parse_error = 1;
								break;
						}
					}
					break;
				}
				case STATE_DONE:
					wdprintf(V_DEBUG, "json", "Done with JSON object!\n");
					break;
			}
		}
	}
	return json;
}

char *json_string_escape_alloc(char *src)
{
	char *res = NULL;
	int   len = src ? strlen(src) : 0;

	if (len > 0) {
		res = malloc(len*2+1); /* len*2 = worst case */
		if (res) {
			int i, j;
			for (i = 0, j = 0; src[i] && j < len*2; i++, j++) {
				if (((unsigned char)src[i]) < 32) {
					res[j] = 32;
				} else if (src[i] == '"' || src[i] == '\\') {
					res[j] = '\\';
					j++;
					res[j] = src[i];
				} else {
					res[j] = src[i];
				}
			}
			res[j] = '\0';
		}
	}
	return res;
}

JSON_Key *json_get_key_object_for_key(JSON_Object *object, char *key)
{
	JSON_Key *res = NULL;
	if (object) {
		JSON_Key *k = object->first_key;
		while (k) {
			if (k->key_name && strcmp(k->key_name, key) == 0) {
				res = k;
				break;
			}
			k = k->next;
		}
	}
	return res;
}

char *json_get_string_value_for_key(JSON_Object *object, char *key)
{
	char *res = NULL;
	if (object) {
		JSON_Key *k = json_get_key_object_for_key(object, key);
		if (k && k->key_name && strcmp(k->key_name, key) == 0 && k->type == STRING) {
			res = k->key_value_str;
		}
	}
	return res;
}

double json_get_number_value_for_key(JSON_Object *object, char *key)
{
	double res = 0.0;
	if (object) {
		JSON_Key *k = json_get_key_object_for_key(object, key);
		if (k && k->key_name && strcmp(k->key_name, key) == 0 && k->type == NUMBER) {
			res = k->key_value_number;
		}
	}
	return res;
}

char *json_get_first_key_string(JSON_Object *object)
{
	return object->first_key->key_name;
}

JSON_Key_Type json_get_type_for_key(JSON_Object *object, char *key)
{
	JSON_Key_Type res = EMPTY;
	if (object) {
		JSON_Key *k = json_get_key_object_for_key(object, key);
		if (k && k->key_name && strcmp(k->key_name, key) == 0) {
			res = k->type;
		}
	}
	return res;
}

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: json.h  Created: 120811
 *
 * Description: A simple JSON parser and helper functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef JSON_H
#define JSON_H
typedef enum JSON_Key_Type { EMPTY, STRING, NUMBER, BOOLEAN, ARRAY, OBJECT } JSON_Key_Type;

typedef struct _JSON_Key JSON_Key;
typedef struct _JSON_Object JSON_Object;

struct _JSON_Key {
	char         *key_name;
	char         *key_value_str;
	double        key_value_number;
	int           key_value_boolean;
	JSON_Object  *key_value_array;
	JSON_Object  *key_value_object;
	JSON_Key_Type type;
	JSON_Key     *next;
};

struct _JSON_Object {
	JSON_Key    *first_key;
	JSON_Object *parent;
	JSON_Object *next; /* for array only */
	int          length;
	int          parse_error;
};

JSON_Object  *json_object_new(JSON_Object *parent);
JSON_Object  *json_parse_alloc(char *json_data);
int           json_object_has_parse_error(JSON_Object *jo);
void          json_keys_free(JSON_Key *first_key);
void          json_object_free(JSON_Object *object);
void          json_object_attach_key(JSON_Object *object, JSON_Key *key);
JSON_Key     *json_key_new(void);
char         *json_string_escape_alloc(char *src);
JSON_Key     *json_get_key_object_for_key(JSON_Object *object, char *key);
char         *json_get_string_value_for_key(JSON_Object *object, char *key);
double        json_get_number_value_for_key(JSON_Object *object, char *key);
char         *json_get_first_key_string(JSON_Object *object);
JSON_Key_Type json_get_type_for_key(JSON_Object *object, char *key);
#endif

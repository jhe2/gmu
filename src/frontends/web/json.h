/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
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

/**
 * Definition of JSON data types. Since JSON does not distinguish between
 * integer and floating point numbers, the JSON_INTEGER and JSON_FLOAT
 * types are for internal use, e.g. to be able to fetch a a number as
 * either an integer or a float. When checking a JSON key for its type,
 * if it is numerical the result will always be JSON_NUMBER.
 */
typedef enum JSON_Key_Type {
	JSON_EMPTY, JSON_STRING, JSON_NUMBER, JSON_INTEGER, JSON_FLOAT, JSON_BOOLEAN, JSON_ARRAY, JSON_OBJECT
} JSON_Key_Type;

typedef struct _JSON_Key JSON_Key;
typedef struct _JSON_Object JSON_Object;

struct _JSON_Key {
	char         *key_name;
	char         *key_value_str;
	double        key_value_number;
	int           key_value_integer;
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
JSON_Object  *json_parse_alloc(const char *json_data);
int           json_object_has_parse_error(JSON_Object *jo);
void          json_keys_free(JSON_Key *first_key);
void          json_object_free(JSON_Object *object);
void          json_object_attach_key(JSON_Object *object, JSON_Key *key);
JSON_Key     *json_key_new(void);
char         *json_string_escape_alloc(const char *src);
JSON_Key     *json_get_key_object_for_key(JSON_Object *object, const char *key);
char         *json_get_string_value_for_key(JSON_Object *object, const char *key);
double        json_get_number_value_for_key(JSON_Object *object, const char *key);
int           json_get_integer_value_for_key(JSON_Object *object, const char *key);
char         *json_get_first_key_string(JSON_Object *object);
JSON_Key_Type json_get_type_for_key(JSON_Object *object, const char *key);
char         *json_encode_message_alloc(JSON_Key_Type type_1, const char *key, ...);
#endif

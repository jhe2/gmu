/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmudecoder.h  Created: 081022
 *
 * Description: Header file for Gmu audio decoders
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _GMUDECODER_H
#define _GMUDECODER_H
#include "reader.h"
#include "wejpconfig.h"

typedef enum GmuMetaDataType {
	GMU_META_TITLE, GMU_META_ARTIST, GMU_META_ALBUM,
	GMU_META_TRACKNR, GMU_META_DATE, GMU_META_COMMENT, GMU_META_LYRICS,
	GMU_META_IMAGE_DATA, GMU_META_IMAGE_DATA_SIZE, GMU_META_IMAGE_MIME_TYPE,
	GMU_META_IS_UPDATED
} GmuMetaDataType;

typedef enum GmuCharset { 
	M_CHARSET_ISO_8859_1, M_CHARSET_ISO_8859_15, M_CHARSET_UTF_8, 
	M_CHARSET_UTF_16_BE, M_CHARSET_UTF_16_LE, M_CHARSET_UTF_16_BOM, 
	M_CHARSET_AUTODETECT
} GmuCharset;

typedef struct _GmuDecoder {
	/* Short identifier such as "vorbis_decoder" */
	const char   *identifier;
	/* Init function. Can be NULL if not neccessary. Will be called ONCE when 
	 * the decoder is loaded. */
	void         (*init_decoder)(void);
	/* Function to be called on unload, you should free/close everything 
	 * that is still allocated/opened at this point. Can be NULL. */
	void         (*close_decoder)(void);
	/* Should return a human-readable name such as "Ogg Vorbis decoder v1.0" */
	const char * (*get_name)(void);
	/* Should return a (rather short) description of the decoder.
	 * \n is allowed in the string. */
	const char * (*get_info)(void);
	/* Must return a semicolon-separated list of file extensions which the
	 * decoder is capable of decoding, e.g. ".mp3;.mp2" */
	const char * (*get_file_extensions)(void);
	/* Should return a list of supported mime-types, semicolon-separated.
	 * This function is optional, but required for http streaming audio. */
	const char * (*get_mime_types)(void);
	/* Open function, parameter is the filename (with absolute path) of 
	 * the file to be decoded. Must return TRUE on success, FALSE otherwise. 
	 * If set_reader_handle() has been defined, the supplied Reader handle should
	 * be used for file/stream access, instead of doing file access with fopen() etc. */
	int          (*open_file)(char *filename);
	/* Function to close the previously opened file, free memory etc. */
	int          (*close_file)(void);
	/* Decodes up to max_size bytes of audio data and writes it to target,
	 * returns actual size */
	int          (*decode_data)(char *target, int max_size);
	/* Seeks in the audio stream, can be NULL if seeking is not supported.
	 * Returns TRUE on success. "second" is the second in the stream to seek to
	 * from the beginning, that is second=0 would be the beginning of the track */
	int          (*seek)(int second);
	/* Returns the current bitrate (in bps) (if available) */
	int          (*get_current_bitrate)(void);
	/* Returns meta data of the track such as artist, title, album, ... 
	 * If for_current_file is TRUE meta data is returned for the current file
	 * otherwise meta data previously loaded with meta_data_load is returned */
	const char * (*get_meta_data)(GmuMetaDataType gmdt, int for_current_file);
	/* Returns meta data similar to the above function, except it returns data
	 * of type 'int' instead of 'char *'. Can be NULL. */
	int          (*get_meta_data_int)(GmuMetaDataType gmdt, int for_current_file);
	/* Returns the sample rate of the current file in Hz such as 44100 */
	int          (*get_samplerate)(void);
	/* Returns the numer of channels of the current file such as 2 for stereo */
	int          (*get_channels)(void);
	/* Returns the length of the current file (in seconds) */
	int          (*get_length)(void);
	/* Returns the (average) overall bitrate of the current file (in bps) */
	int          (*get_bitrate)(void);
	/* Returns the file type of the current file (such as "Ogg Vorbis") */
	const char * (*get_file_type)(void);
	/* Returns the buffer size (in bytes) used by the decoder (e.g. 2048) */
	int          (*get_decoder_buffer_size)(void);
	/* Loads the meta data for a given file only. No decoding is done.
	 * Returns 1 on success, 0 otherwise. Can be NULL if not available. */
	int          (*meta_data_load)(const char *filename);
	/* Clean up function for the meta data loader. You should free any memory
	 * allocated by meta_data_load and close any files opened by meta_data_load
	 * here. Returns TRUE on success. Can be NULL not neccessary. */
	int          (*meta_data_close)(void);
	/* Returns the charset encoding of the meta data. Valid values are
	 * M_CHARSET_UTF_8, M_CHARSET_ISO_8859_1, M_CHARSET_UTF_16_BOM, 
	 * M_CHARSET_UTF_16_BE, M_CHARSET_UTF_16_LE and M_CHARSET_AUTODETECT.
	 * With M_CHARSET_AUTODETECT Gmu tries to detect the used charset itself. */
	GmuCharset   (*meta_data_get_charset)(void);
	/* Checks wether the supplied data contains data compatible with the decoder.
	 * This function is optional, but is recommended for streaming audio. Returns 1
	 * on success and 0 otherwise. */
	int          (*data_check_magic_bytes)(const char *data, int size);
	/* Supplies a Reader handle for reading file/stream data. This function is
	 * optional, but required for http streaming audio. If this function is not NULL
	 * the decoder has to close the supplied handle, when finished. */
	void         (*set_reader_handle)(Reader *r);
	/* internal handle, do not use */
	void         *handle;
} GmuDecoder;

/* This function must be implemented by the decoder. It must return a valid
 * GmuDecoder object */
GmuDecoder *GMU_REGISTER_DECODER(void);
#endif

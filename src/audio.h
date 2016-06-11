/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: audio.h  Created: 061110
 *
 * Description: Audio output functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#define MIN_BUFFER_FILL 32768
#define AUDIO_MAX_SW_VOLUME 16
#ifndef _AUDIO_H
#define _AUDIO_H
#include <sys/types.h>

int      audio_device_open(int samplerate, int channels);
int      audio_fill_buffer(char *data, size_t size);
int      audio_get_playtime(void);
void     audio_buffer_init(void);
void     audio_buffer_clear(void);
void     audio_buffer_free(void);
void     audio_device_close(void);
size_t   audio_buffer_get_fill(void);
size_t   audio_buffer_get_size(void);
int      audio_get_status(void);
void     audio_force_pause(int pause);
int      audio_set_pause(int pause_state);
int      audio_get_pause(void);
void     audio_set_volume(int vol); /* 0..15 */
int      audio_get_volume(void);
long     audio_set_sample_counter(long sample);
long     audio_increase_sample_counter(long sample_offset);
long     audio_get_sample_count(void);
void     audio_wait_until_more_data_is_needed(void);
void     audio_set_done(void);
void     audio_set_fade_volume(int percent);
int      audio_fade_out_step(unsigned int step_size);
void     audio_reset_fade_volume(void);
int      audio_fade_out_in_progress(void);
int16_t *audio_spectrum_get_current_amplitudes(void);
void     audio_spectrum_register_for_access(void);
void     audio_spectrum_unregister(void);
int      audio_spectrum_read_lock(void);
void     audio_spectrum_read_unlock(void);
#endif

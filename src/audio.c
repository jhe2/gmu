/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: audio.c  Created: 061110
 *
 * Description: Audio output functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL.h"
#include "ringbuffer.h"
#include "audio.h"
#include "hw.h"
#define RINGBUFFER_SIZE 131072

static RingBuffer  audio_rb;
static SDL_mutex  *audio_mutex;
static long        buf_read_counter;
static int         have_samplerate, have_channels;
static int         device_open;
static int         paused;
static char        buf[65536];
static int         volume, volume_internal;

int audio_fill_buffer(char *data, int size)
{
	int result;
	while (SDL_mutexP(audio_mutex) == -1) SDL_Delay(50);
	result = ringbuffer_write(&audio_rb, data, size);
	while (SDL_mutexV(audio_mutex) == -1) SDL_Delay(50);
	return result;
}

static void fill_audio(void *udata, Uint8 *stream, int len)
{
	while (SDL_mutexP(audio_mutex) == -1) SDL_Delay(50);
	if (ringbuffer_get_fill(&audio_rb) < MIN_BUFFER_FILL) {
		printf("audio: Buffer (almost) empty! Buffer fill: %d bytes\n", 
		       RINGBUFFER_SIZE - ringbuffer_get_free(&audio_rb));
		SDL_PauseAudio(1);
		while (SDL_mutexV(audio_mutex) == -1) SDL_Delay(50);
		while (ringbuffer_get_free(&audio_rb) > 65536) SDL_Delay(60);
		while (SDL_mutexP(audio_mutex) == -1) SDL_Delay(50);
		if (!paused) SDL_PauseAudio(0);
	}
	buf_read_counter += len;
	ringbuffer_read(&audio_rb, buf, len);
	SDL_MixAudio(stream, (unsigned char *)buf, len, volume);
	while (SDL_mutexV(audio_mutex) == -1) SDL_Delay(50);
}

int audio_device_open(int samplerate, int channels)
{
	static SDL_AudioSpec wanted, obtained;
	int                  result = -1;

	buf_read_counter = 0;
	/* Keep audio device open unless sampling rate or number of channels change */
	printf("audio: Device already open: %s\n", device_open ? "yes" : "no");
	if (device_open)
		printf("audio: Samplerate: have=%d want=%d Channels: have=%d want=%d\n",
		       have_samplerate, samplerate, have_channels, channels);
	if (!device_open || samplerate != have_samplerate || channels != have_channels) {
		if (device_open) audio_device_close();
		printf("audio: Opening audio device...\n");
		wanted.freq     = samplerate;
		wanted.format   = AUDIO_S16;
		wanted.channels = channels; /* 1 = mono, 2 = stereo */
		wanted.samples  = SAMPLE_BUFFER_SIZE;
		wanted.callback = fill_audio;
		wanted.userdata = NULL;
		if (SDL_OpenAudio(&wanted, &obtained) < 0) {
			printf("audio: Could not open audio: %s\n", SDL_GetError());
			result = -3;
		} else {
			result = 0;
			device_open = 1;
			have_samplerate = samplerate;
			have_channels   = channels;
			printf("audio: Device opened with %d Hz, %d channels and sample buffer w/ %d samples.\n",
			        obtained.freq, obtained.channels, obtained.samples);
		}
	} else {
		printf("audio: Using already opened audio device with the same settings...\n");
		result = 0;
	}
	ringbuffer_clear(&audio_rb);
	if (result == 0)
		SDL_PauseAudio(1);
	return result;
}

int audio_set_pause(int pause_state)
{
	if (device_open) {
		printf("audio: %s\n", pause_state ? "Pause!" : "Play!");
		if (paused != pause_state) {
			paused = pause_state;
			SDL_PauseAudio(paused);
		}
	} else {
		printf("audio: Device not opened. Cannot set pause state!\n");
	}
	return paused;
}

int audio_get_pause(void)
{
	return paused;
}

int audio_get_playtime(void)
{
	return buf_read_counter / (have_samplerate * 2 * have_channels) * 1000;
}

int audio_buffer_get_fill(void)
{
	return ringbuffer_get_fill(&audio_rb);
}

int audio_buffer_get_size(void)
{
	return ringbuffer_get_size(&audio_rb);
}

void audio_buffer_init(void)
{
	volume = SDL_MIX_MAXVOLUME;
	volume_internal = 15;
	paused = 1;
	device_open = 0;
	have_samplerate = 1;
	have_channels = 1;
	ringbuffer_init(&audio_rb, RINGBUFFER_SIZE);
	audio_mutex = SDL_CreateMutex();
}

void audio_buffer_clear(void)
{
	audio_set_pause(1);
	ringbuffer_clear(&audio_rb);
}

void audio_buffer_free(void)
{
	ringbuffer_free(&audio_rb);
	SDL_DestroyMutex(audio_mutex);
}

void audio_device_close(void)
{
	if (device_open) {
		printf("audio: Closing device.\n");
		audio_set_pause(1);
		device_open = 0;
		SDL_CloseAudio();
		printf("audio: Device closed.\n");
	}
}

static const int volume_array[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 24, 32, 48, 64, 96, 128 };

void audio_set_volume(int vol) /* 0..AUDIO_MAX_SW_VOLUME */
{
	volume_internal = (vol < AUDIO_MAX_SW_VOLUME ? vol : AUDIO_MAX_SW_VOLUME-1);
	volume_internal = (volume_internal > 0 ? volume_internal : 0);
	volume = volume_array[volume_internal]; /*(SDL_MIX_MAXVOLUME / AUDIO_MAX_SW_VOLUME) * volume_internal;*/
	printf("audio: volume=%d (%d/%d)\n", volume, SDL_MIX_MAXVOLUME, AUDIO_MAX_SW_VOLUME);
}

int audio_get_volume(void)
{
	return volume_internal;
}

long audio_set_sample_counter(long sample)
{
	buf_read_counter = (sample * 2 * have_channels);
	return buf_read_counter;
}

long audio_increase_sample_counter(long sample_offset)
{
	buf_read_counter += (sample_offset * 2 * have_channels);
	if (buf_read_counter < 0) buf_read_counter = 0;
	return buf_read_counter;
}

long audio_get_sample_count(void)
{
	return buf_read_counter / (2 * have_channels);
}

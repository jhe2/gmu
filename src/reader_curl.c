/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2026 Johannes Heimansberg (wej.k.vu)
 *
 * File: reader_curl.c  Created: 070426
 *
 * Description: File/Stream reader functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

// This file is used for reader if URL_WITH_CURL is defined
#ifdef URL_WITH_CURL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "util.h" /* for assign_signal_handler() */
#include "reader.h"
#include "reader_curl.h"
#include "ringbuffer.h"
#include "debug.h"
#include "core.h" /* for VERSION_NUMBER and DEFAULT_THREAD_STACK_SIZE */
#include "pthread_helper.h"

#include <curl/curl.h> // Use tiny-curl lib for https stream support.

extern size_t http_cache_size;           
extern size_t http_cache_prebuffer_size; 

static size_t gmu_curl_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
	size_t total_bytes = size * nmemb;
	Reader *r = (Reader *)userdata;
	int write_okay = 0;

	// Handle 0-byte edge cases cleanly
	if (total_bytes == 0) return 0;

	/* 
	 * 1. THE WRITE & THROTTLE LOOP
	 * Keep looping until Gmu's buffer accepts the data chunk, 
	 * or until the main thread flags an exit request (r->eof).
	 */
	while (!write_okay && !r->eof) {
		pthread_mutex_lock(&(r->mutex));
		// Note: Gmu's native logic passes the whole buffer chunk at once
		write_okay = ringbuffer_write(&(r->rb_http), (char *)ptr, total_bytes);
		pthread_mutex_unlock(&(r->mutex));

		// Inside your original write callback loop where ringbuffer_write succeeds:
		if (!write_okay) {
			// Buffer is full. Yield CPU to let the decoder catch up.
			// Using Gmu's original 1500us (1.5ms) delay timing.
			usleep(1500);
		}
	}

	/* 
	 * 2. INTERRUPT CHECK
	 * If the loop broke because the user pressed stop (r->eof became 1),
	 * return 0 to tell cURL to immediately abort its active network stream.
	 */
	if (r->eof) {
		return 0; 
	}

	/* 
	 * 3. PRE-BUFFERING HANDSHAKE
	 * If Gmu is waiting for the initial buffer to fill, check if we hit the limit yet.
	 */
	if (!r->is_ready && ringbuffer_get_fill(&(r->rb_http)) >= http_cache_prebuffer_size) {
		r->is_ready = 1;
	}

	/* 
	 * 4. LIVE LOGGING
	 * Keep Gmu's original console buffer tracker functional.
	 */
	wdprintf(V_DEBUG, "reader", "buf fill: %d bytes\r", ringbuffer_get_fill(&(r->rb_http)));
	fflush(stdout);

	// Tell cURL we processed all incoming bytes successfully
	return total_bytes; 
}

static size_t gmu_curl_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
	size_t total_bytes = size * nitems;
	Reader *r = (Reader *)userdata;
	
	// Libcurl passes blank lines ("\r\n") at the end of headers; skip them
	if (total_bytes <= 2 || buffer[0] == '\r' || buffer[0] == '\n') {
		wdprintf(V_DEBUG, "reader", "End of Header.  %d bytes\n", total_bytes);  
		pthread_mutex_lock(&(r->mutex));
		r->header_end_found = 1;
		pthread_cond_signal(&(r->cond));
		pthread_mutex_unlock(&(r->mutex));
		return total_bytes;
	}

	// Find the colon separating key and value
	char *colon = memchr(buffer, ':', total_bytes);
	if (colon) {
		char key[256];
		char value[512];
		
		// Calculate lengths safely within bounds
		size_t key_len = colon - buffer;
		if (key_len > 255) key_len = 255;
		
		// Extract and null-terminate the key
		memcpy(key, buffer, key_len);
		key[key_len] = '\0';
		
		// Skip colon and any leading spaces for the value
		char *val_start = colon + 1;
		while (val_start < buffer + total_bytes && *val_start == ' ') {
			val_start++;
		}
		
		// Calculate value length, cutting off trailing \r or \n
		char *val_end = buffer + total_bytes;
		while (val_end > val_start && (*(val_end - 1) == '\r' || *(val_end - 1) == '\n')) {
			val_end--;
		}
		
		size_t val_len = val_end - val_start;
		if (val_len > 511) val_len = 511;
		
		if (val_len > 0) {
			memcpy(value, val_start, val_len);
			value[val_len] = '\0';
			
			// Replicate Gmu's original config management behavior
			wdprintf(V_DEBUG, "reader", "key=[%s]\n", key);
			wdprintf(V_DEBUG, "reader", "value=[%s]\n", value);
			cfg_add_key(r->streaminfo, key, value);
		}
	}
	
	wdprintf(V_INFO, "reader", "total_bytes=%d.\n", total_bytes);
	return total_bytes;
}

// Libcurl progress callback (Runs multiple times per second during transfer/handshakes)
static int gmu_curl_progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	Reader *r = (Reader *)clientp;

	// If Gmu main thread flagged an abort or EOF, return non-zero to terminate cURL instantly
	if (r && r->eof) {
		wdprintf(V_DEBUG, "reader", "Progress callback intercepted r->eof. Aborting curl stream.\n");
		return 1; 
	}
	
	return 0; // 0 means continue running normally
}

static void *gmu_curl_reader_thread(void *arg)
{
	Reader *r = (Reader *)arg;

	CURL *curl = curl_easy_init();
	if (!curl) {
		r->eof = 1;
		wdprintf(V_DEBUG, "reader", "curl_easy_init failed\n"); 
		return NULL;
	}

	// Replicate Gmu's original custom User-Agent
	char user_agent_buf[64];
	snprintf(user_agent_buf, sizeof(user_agent_buf), "Gmu/%s", VERSION_NUMBER);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_buf);

	// Inject the custom ICY metadata request header
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Icy-MetaData: 1");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	// Configure connection settings
	curl_easy_setopt(curl, CURLOPT_URL, r->url); // Ensure r->url is populated in _reader_open
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	// Fast disconnect & dead stream timeouts
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6L);   // Max 6 seconds to connect
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);  // Below 1 byte/sec...
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);   // ...for 5 seconds = dead stream.
	//curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);    // maintain a stable stream state

	// Progress function for instant user interrupt 
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, gmu_curl_progress_callback);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, r);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);       // Must be 0L to activate callback!

	// Register the header callback
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, gmu_curl_header_callback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, r);

	// Route audio data directly to your original ring buffer writer
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gmu_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, r);

	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); // Abort on 403, 404,.. errors.

	// Run the connection blocking loop
	CURLcode res = curl_easy_perform(curl);

	if (res == CURLE_HTTP_RETURNED_ERROR) { /* Report 403, 404,.. errors */
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		wdprintf(V_ERROR, "reader", "HTTP error %ld on %s\n", http_code, r->url);
	}
	
	// Clean up allocations safely for the Zipit Z2
	curl_easy_cleanup(curl);
	if (headers) {
		curl_slist_free_all(headers);
	}
	
	wdprintf(V_DEBUG, "reader", "thread done.\n");
	pthread_mutex_lock(&(r->mutex));
	r->eof = 1; // Mark EOF only after cURL is completely finished
	// cond_signal in case thread ended quickly, while curl_reader_open() is waiting on headers.
	pthread_cond_signal(&(r->cond));  
	pthread_mutex_unlock(&(r->mutex));
	
	return NULL;
}

Reader *reader_open_curl(Reader *r, const char *url, int max_redirects)
{
	pthread_cond_init(&(r->cond), NULL);		
	r->header_end_found = 0;
	/* SAVE THE URL RIGHT AWAY */
	strncpy(r->url, url, sizeof(r->url) - 1);
	r->url[sizeof(r->url) - 1] = '\0'; // Ensure null-termination

	wdprintf(V_DEBUG, "reader", "curl_reader_thread\n");  
	assign_signal_handler(SIGPIPE, SIG_IGN);  // avoid termination on socket drops
	/* Start reader thread... */
	// NOTE:  512K bytes is well over 10 secs for a 320K bps stream.  Longer for most radio streams.
	// if (ringbuffer_init(&(r->rb_http), 32768)) { // AI wanted 32K (much less than 512K)
	if (ringbuffer_init(&(r->rb_http), http_cache_size)) { 
		// if (pthread_create(&(r->thread), NULL, gmu_curl_reader_thread, r) != 0) { // AI wanted this
		if (pthread_create_with_stack_size(&(r->thread), DEFAULT_THREAD_STACK_SIZE, gmu_curl_reader_thread, r) == 0) {
			struct timespec ts;
			int wait_result = 0;
			long timeout_seconds = 10; /* Max time allowed to receive headers */

			pthread_mutex_lock(&r->mutex);
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += timeout_seconds;

			while (!r->header_end_found && !wait_result && !r->eof) {
				/* Sleep here until the header callback signals us */
				wait_result = pthread_cond_timedwait(&r->cond, &r->mutex, &ts);
			}
			pthread_mutex_unlock(&r->mutex);
			if (wait_result == ETIMEDOUT) {
				wdprintf(V_DEBUG, "reader", "Timed out waiting for HTTP headers!\n");
			}
			else if (r->eof) {
				wdprintf(V_DEBUG, "reader", "EOF waiting for HTTP headers!\n");
			}
			/* Try to figure out stream length */
			if (r->header_end_found) {
				char *val = cfg_get_key_value_ignore_case(r->streaminfo, "Content-Length");
				if (val) {
					r->file_size = (size_t)atol(val);
					wdprintf(V_DEBUG, "reader", "Stream size = %d bytes.\n", r->file_size);
				}
				return r;  /* === ALL GOOD ===  Return r and skip the failure handlers below. */
			}
			else {
				wdprintf(V_DEBUG, "reader", "No header end found.\n", r->file_size);
			}
			reader_close(r); /* We started the thread, but failed to read the headers.  Abandon ship.*/
			return NULL;
		} else {
			wdprintf(V_ERROR, "reader", "pthread_create failed.\n");
		}
	} else {
		wdprintf(V_ERROR, "reader", "Out of memory.\n");
	}
	pthread_mutex_destroy(&(r->mutex));
	cfg_free(r->streaminfo);
	free(r);
	return NULL;
}

#endif /* URL_WITH_CURL */

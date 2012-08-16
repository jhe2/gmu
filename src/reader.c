/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: reader.c  Created: 110406
 *
 * Description: File/Stream reader functions
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
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "reader.h"
#include "ringbuffer.h"
#include "debug.h"
#include "core.h" /* for VERSION_NUMBER */

static int http_cache_size = 512 * 1024;
static int http_cache_prebuffer_size = 256 * 1024;

int reader_set_cache_size_kb(int size, int prebuffer_size)
{
	size = size < HTTP_CACHE_SIZE_MIN_KB ? HTTP_CACHE_SIZE_MIN_KB : size;
	size = size > HTTP_CACHE_SIZE_MAX_KB ? HTTP_CACHE_SIZE_MAX_KB : size;
	http_cache_size = size * 1024;
	prebuffer_size *= 1024;
	if (prebuffer_size > 0 && prebuffer_size <= http_cache_size / 2 + http_cache_size / 4)
		http_cache_prebuffer_size = prebuffer_size;
	wdprintf(V_INFO, "reader", "Cache size: %d kB\n", size);
	wdprintf(V_INFO, "reader", "Cache prebuffer size: %d kB\n", prebuffer_size / 1024);
	return size;
}

int reader_get_cache_fill(Reader *r)
{
	return ringbuffer_get_fill(&(r->rb_http));
}

/* get sockaddr, IPv4 or IPv6 */
static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int http_url_split_alloc(char *url, char **hostname, int *port, char **path)
{
	int len = url ? strlen(url) : 0;
	if (len > 7) {
		char *host_begin = url+7;
		char *port_begin = strchr(host_begin, ':');
		char *path_tmp = NULL;
		int   path_len = 0, host_len;

		if (port_begin) {
			*port = atoi(port_begin+1);
		} else {
			port_begin = strchr(host_begin, '/');
			*port = 80; /* default http port */
		}
		if (port_begin) path_tmp = strchr(port_begin, '/');
		host_len = (port_begin ? port_begin - host_begin : len-7);

		*hostname = malloc(host_len+1);
		if (*hostname) {
			strncpy(*hostname, host_begin, host_len);
			(*hostname)[host_len] = '\0';
		}
		if (!path_tmp) path_tmp = "/";
		path_len = strlen(path_tmp);
		*path = malloc(path_len+1);
		if (*path) {
			strncpy(*path, path_tmp, path_len);
			(*path)[path_len] = '\0';
		}
	}
	return 0;
}

static void *http_reader_thread(void *arg)
{
	Reader *r = (Reader *)arg;
	int numbytes = 1, err = 0;
	char buf[4096];

	while (numbytes != -1 && numbytes > 0 && !r->eof) {
		do {
			numbytes = recv(r->sockfd, buf, 4096, 0);
			err = errno;
			if (numbytes > 0) { /* write to ringbuffer */
				int write_okay = 0;
				while (!write_okay && !r->eof) {
					pthread_mutex_lock(&(r->mutex));
					write_okay = ringbuffer_write(&(r->rb_http), buf, numbytes);
					pthread_mutex_unlock(&(r->mutex));
					usleep(1500);
				}
			} else {
				usleep(300000);
				if (err == 0) {
					if (numbytes == 0) r->eof = 1;
				} else {
					wdprintf(V_DEBUG, "reader", "Network problem: %s (%d)\n", strerror(err), err);
					wdprintf(V_DEBUG, "reader", "Retrying...\n");
				}
			}
		} while (numbytes <= 0 && !r->eof && ringbuffer_get_fill(&(r->rb_http)) > 4000);
		wdprintf(V_DEBUG, "reader", "buf fill: %d bytes\r", ringbuffer_get_fill(&(r->rb_http)));
		if (!r->is_ready && ringbuffer_get_fill(&(r->rb_http)) >= http_cache_prebuffer_size)
			r->is_ready = 1;
		fflush(stdout);
	}
	wdprintf(V_DEBUG, "reader", "thread done.\n");
	r->eof = 1;
	return NULL;
}

int reader_is_ready(Reader *r)
{
	return r->is_ready;
}

/* Opens a local file or HTTP URL for reading */
static Reader *_reader_open(char *url, int max_redirects)
{
	Reader *r = malloc(sizeof(Reader));
	if (r) {
		r->eof = 0;
		r->file = NULL;
		r->sockfd = 0;
		r->seekable = 0;
		r->buf = NULL;
		r->buf_size = 0;
		r->buf_data_size = 0;
		r->file_size = 0;
		r->is_ready = 0;
		wdprintf(V_DEBUG, "reader", "mutex init.\n");
		pthread_mutex_init(&(r->mutex), NULL);
		wdprintf(V_DEBUG, "reader", "mutex init done.\n");

		cfg_init_config_file_struct(&(r->streaminfo));

		if (strncasecmp(url, "http://", 7) == 0) { /* Got a HTTP URL */
			char *hostname = NULL, *path = NULL;
			int   port = 80;
			/* open http stream... */
			/* 1) Split URL into host, port and path */
			http_url_split_alloc(url, &hostname, &port, &path);
			/* 2) open connection to host on port */
			wdprintf(V_INFO, "reader", "Opening connection to host %s on port %d. Reading from %s.\n", hostname, port, path);
			if (hostname && path && port > 0) {
				struct addrinfo hints, *servinfo, *p;
				int    rv;
				char   s[INET6_ADDRSTRLEN];
				char   port_str[6];

				snprintf(port_str, 5, "%d", port);
				memset(&hints, 0, sizeof hints);
				hints.ai_family = AF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;

				if ((rv = getaddrinfo(hostname, port_str, &hints, &servinfo)) != 0) {
					wdprintf(V_ERROR, "reader",  "getaddrinfo: %s\n", gai_strerror(rv));
					free(r);
					r = NULL;
				} else {
					int flags = 0;
					/* loop through all the results and connect to the first we can */
					for (p = servinfo; p != NULL; p = p->ai_next) {
						if ((r->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
							wdprintf(V_INFO, "reader", "socket: %s\n", strerror(errno));
							continue;
						} else { /* Set socket timeout to 2 seconds */
							struct timeval tv;
							tv.tv_sec = 2;
							tv.tv_usec = 0;
							if (setsockopt(r->sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof tv)) {
								wdprintf(V_INFO, "reader", "setsockopt: %s\n", strerror(errno));
							}
						}

						flags = fcntl(r->sockfd, F_GETFL, 0);
						fcntl(r->sockfd, F_SETFL, flags | O_NONBLOCK);
						if (connect(r->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
							wdprintf(V_INFO, "reader", "connect: %s\n", strerror(errno));
							if (errno == EINPROGRESS)
								break;
							else
								close(r->sockfd);
							continue;
						}
						break;
					}

					if (errno == EINPROGRESS) {
						fd_set myset;
						struct timeval tv; 

						wdprintf(V_DEBUG, "reader", "Connection in progress. select()ing...\n");
						do {
							int       res, valopt; 
							socklen_t lon;

							tv.tv_sec = 5; 
							tv.tv_usec = 0; 
							FD_ZERO(&myset); 
							FD_SET(r->sockfd, &myset); 
							res = select((r->sockfd)+1, NULL, &myset, NULL, &tv); 
							if (res < 0 && errno != EINTR) {
								wdprintf(V_DEBUG, "reader", "Error while connecting: %d - %s\n", errno, strerror(errno));
								break;
							} else if (res > 0) {
								lon = sizeof(int);
								if (getsockopt(r->sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0) {
									wdprintf(V_DEBUG, "reader", "Error in getsockopt(): %d - %s\n", errno, strerror(errno));
									p = NULL;
									break;
								}
								if (valopt) {
									wdprintf(V_DEBUG, "reader", "Error in delayed connection(): %d - %s\n", valopt, strerror(valopt));
									p = NULL;
								}
								break;
							} else {
								wdprintf(V_DEBUG, "reader", "Timeout in select() - Cancelling!\n");
								p = NULL;
								break;
							}
						} while (1);
						flags = fcntl(r->sockfd, F_GETFL, 0);
						fcntl(r->sockfd, F_SETFL, flags & (~O_NONBLOCK));
					}

					if (p == NULL) {
						wdprintf(V_ERROR, "reader", "Failed to connect.\n");
						free(r);
						r = NULL;
					} else {
						inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
						wdprintf(V_INFO, "reader", "Connected to %s:%d.\n", s, port);
						/* Send HTTP GET request */
						{
							char http_request[512];
							snprintf(http_request, 511,
							         "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: Gmu/%s\r\nIcy-MetaData: 1\r\n\r\n",
							         path, hostname, VERSION_NUMBER);
							wdprintf(V_DEBUG, "reader", "Sending request: %s\n", http_request);
							send(r->sockfd, http_request, strlen(http_request), 0);
						}

						/* Start reader thread... */
						if (ringbuffer_init(&(r->rb_http), http_cache_size)) {
							pthread_create(&(r->thread), NULL, http_reader_thread, r);
						} else {
							wdprintf(V_ERROR, "reader", "Out of memory.\n");
						}

						/* Skip http response header */
						{
							int  header_end_found = 0, cnt = 0, i = 0;
							char ch, key[256], value[512];
							
							key[0] = '\0'; value[0] = '\0';
							/* Search for http header end sequence "\r\n\r\n" (or "\n\n"); I assume http headers to be no longer than 32 kB. */
							while (!header_end_found && !reader_is_eof(r) && cnt < 32768) {
								/* extract key */
								ch = 0;
								while (ch != '\r' && ch != '\n' && ch != ':' && !reader_is_eof(r)) {
									ch = reader_read_byte(r);
									if (i < 255 && ch != ':' && ch != '=' && ch != '\r' && ch != '\n') key[i++] = ch;
									cnt++;
								}
								key[i] = '\0';
								wdprintf(V_DEBUG, "reader", "key=[%s]\n", key);

								/* extract value */
								i = 0;
								while ((ch = reader_read_byte(r)) == ' ' && !reader_is_eof(r)) cnt++; /* skip spaces after ":" */
								if (ch != '\r' && ch != '\n') value[i++] = ch;
								while (ch != '\r' && ch != '\n' && !reader_is_eof(r)) {
									ch = reader_read_byte(r);
									if (i < 511 && ch != '\r' && ch != '\n') value[i++] = ch;
									cnt++;
								}
								value[i] = '\0';
								wdprintf(V_DEBUG, "reader", "value=[%s]\n", value);
								if (key[0] && value[0])
									cfg_add_key(&(r->streaminfo), key, value);

								i = 0;
								if (ch == '\n' || (ch = reader_read_byte(r)) == '\n') {
									ch = reader_read_byte(r);
									if ((ch == '\r' && (ch = reader_read_byte(r)) == '\n') || ch == '\n')
										header_end_found = 1;
									else
										key[i++] = ch;
									cnt+=3;
								} else {
									key[i++] = ch;
								}
							}
							wdprintf(V_DEBUG, "reader", "HTTP header skipped: %s (%d bytes)\n", header_end_found ? "yes" : "no", cnt);
							/* Try to figure out stream length */
							if (header_end_found) {
								char *val = cfg_get_key_value_ignore_case(r->streaminfo, "Content-Length");
								if (val) {
									r->file_size = atol(val);
									wdprintf(V_DEBUG, "reader", "Stream size = %d bytes.\n", r->file_size);
								}
							}
						}
					}
					freeaddrinfo(servinfo);
				}
			}
			if (hostname) free(hostname);
			if (path)     free(path);
			/* Check for 302 redirect (Location) */
			if (r) {
				char *v = cfg_get_key_value_ignore_case(r->streaminfo, "Location");
				if (v) {
					int   len = strlen(v);
					char *vc = NULL;
					
					if (len > 0 && (vc = malloc(len+1))) {
						strncpy(vc, v, len);
						vc[len] = '\0';
					}
					wdprintf(V_INFO, "reader", "302 Redirect found: %s\n", vc ? vc : "unknown");
					reader_close(r);
					if (max_redirects > 0 && vc) {
						r = _reader_open(vc, max_redirects-1);
					} else {
						wdprintf(V_WARNING, "reader", "Too many HTTP redirects.\n");
					}
					if (vc) free(vc);
				}
			}
		} else { /* Treat everything else as a local file (for now) */
			wdprintf(V_INFO, "reader", "Opening file %s.\n", url);
			r->file = fopen(url, "r");
			if (r->file) {
				struct stat st;
				r->seekable = 1;
				if (stat(url, &st) == 0) {
					r->file_size = st.st_size;
					wdprintf(V_DEBUG, "reader", "File size = %d bytes.\n", r->file_size);
				}
				r->is_ready = 1;
			} else {
				wdprintf(V_ERROR, "reader", "Unable to open file '%s'.\n", url);
				free(r);
				r = NULL;
			}
		}
	}
	return r;
}

Reader *reader_open(char *url)
{
	return _reader_open(url, 3);
}

int reader_close(Reader *r)
{
	if (r) {
		if (r->file) { /* local file */
			fclose(r->file);
		} else if (r->sockfd > 0) { /* http stream */
			/* close http stream */
			close(r->sockfd);
			r->eof = 1;
			wdprintf(V_DEBUG, "reader", "Waiting for reader thread to finish.\n");
			pthread_join(r->thread, NULL);
			wdprintf(V_DEBUG, "reader", "Reader thread joined.\n");
			ringbuffer_free(&(r->rb_http));
		}
		pthread_mutex_destroy(&(r->mutex));
		if (r->buf) free(r->buf);
		cfg_free_config_file_struct(&(r->streaminfo));
		free(r);
		r = NULL;
	}
	return 0;
}

int reader_is_eof(Reader *r)
{
	return r->file ? r->eof : ringbuffer_get_fill(&(r->rb_http)) > 0 ? 0 : r->eof;
}

char reader_read_byte(Reader *r)
{
	int ch = 0;
	if (r->file) {
		ch = fgetc(r->file);
		if (ch == EOF) r->eof = 1;
	} else {
		int read_okay = 0;
		while (!read_okay && !reader_is_eof(r)) {
			char buf[1];
			pthread_mutex_lock(&(r->mutex));
			read_okay = ringbuffer_read(&(r->rb_http), buf, 1);
			pthread_mutex_unlock(&(r->mutex));
			if (!read_okay && r->eof) break;
			if (!read_okay) usleep(150);
			ch = buf[0];
		}
	}
	return (char)ch;
}

int reader_get_number_of_bytes_in_buffer(Reader *r)
{
	return r->buf_data_size;
}

int reader_read_bytes(Reader *r, int size)
{
	int read_okay = 0;
	if (size > r->buf_size) r->buf = realloc(r->buf, size+1);
	if (r->buf) r->buf_size = size;
	if (r->file) {
		if (r->buf) {
			long pos = ftell(r->file);
			memset(r->buf, 0, r->buf_size);
			if (fread(r->buf, size, 1, r->file) < 1) {
				do { /* Not the most elegant solution.. ;) */
					size--;
					if (fseek(r->file, pos, SEEK_SET) != 0)
						wdprintf(V_ERROR, "reader", "Unable to seek to pos %d:(\n", pos);
				} while (fread(r->buf, size, 1, r->file) < 1 && size > 0);
				r->eof = feof(r->file);
				if (size > 0)
					read_okay = 1;
				else
					r->eof = 1;
				r->buf_data_size = size;
			} else {				
				r->buf[size] = '\0';
				read_okay = 1;
				r->buf_data_size = size;
			}
		}
	} else {
		while (!read_okay) {
			pthread_mutex_lock(&(r->mutex));
			read_okay = ringbuffer_read(&(r->rb_http), r->buf, size);
			pthread_mutex_unlock(&(r->mutex));
			if (read_okay) r->buf_data_size = size; else r->buf_data_size = 0;
			if (!read_okay && r->eof) break;
			r->buf[size] = '\0';
			if (!read_okay) usleep(150);
		}
	}
	return read_okay;
}

char *reader_get_buffer(Reader *r)
{
	return r->buf;
}

long reader_get_file_size(Reader *r)
{
	return r->file_size;
}

/* Resets the stream to the beginning (if possible), returns 1 on success, 0 otherwise */
int reader_reset_stream(Reader *r)
{
	int res = 0;
	if (r->file) { /* Only possible for local files */
		rewind(r->file);
		res = 1;
	}
	return res;
}

int reader_is_seekable(Reader *r)
{
	return r->seekable;
}

int reader_seek(Reader *r, int byte_offset)
{
	int res = 0;
	if (r->file) {
		if (fseek(r->file, byte_offset, SEEK_SET) == 0) {
			r->buf_data_size = 0;
			res = 1;
		} else {
			wdprintf(V_INFO, "reader", "Seeking failed. :(\n");
		}
	} else {
		/* Seeking not possible in HTTP streams */
		res = 0;
	}
	return res;
}

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
#include "reader.h"
#include "ringbuffer.h"

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
		int   path_len = 0;

		if (port_begin) {
			*port = atoi(port_begin+1);
		} else {
			port_begin = strchr(host_begin, '/');
			*port = 80; /* default http port */
		}
		if (port_begin) path_tmp = strchr(port_begin, '/');
		int host_len = (port_begin ? port_begin - host_begin : len-7);

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
	int numbytes = 1;
	char buf[4096];

	r->intbuf_data_size = 0;
	while (numbytes != -1 && numbytes > 0 && !r->eof) {
		numbytes = recv(r->sockfd, buf, 4096, 0);
		if (numbytes > 0) { /* write to ringbuffer */
			int write_okay = 0;
			while (!write_okay) {
				pthread_mutex_lock(&(r->mutex));
				write_okay = ringbuffer_write(&(r->rb_http), buf, numbytes);
				pthread_mutex_unlock(&(r->mutex));
				usleep(150);
			}
		}
		printf("reader thread: ring buffer fill: %d bytes\r", ringbuffer_get_fill(&(r->rb_http)));
		fflush(stdout);
	}
	printf("reader thread done.\n");
	r->eof = 1;
	return NULL;
}

/* Opens a local file or HTTP URL for reading */
Reader *reader_open(char *url)
{
	Reader *r = malloc(sizeof(Reader));
	if (r) {
		r->intbuf = NULL;
		r->intbuf_size = 0;
		r->intbuf_data_size = 0;
		r->intbuf_pos = 0;
		r->in_first_buffer = 1;
		r->http_payload_start = NULL;
		r->eof = 0;
		r->file = NULL;
		r->sockfd = 0;
		r->seekable = 0;
		r->buf = NULL;
		r->buf_size = 0;
		r->buf_data_size = 0;
		if (strncmp(url, "http://", 7) == 0) { /* Got a HTTP URL */
			char *hostname = NULL, *path = NULL;
			int   port = 80;
			/* open http stream... */
			/* 1) Split URL into host, port and path */
			http_url_split_alloc(url, &hostname, &port, &path);
			/* 2) open connection to host on port */
			printf("reader: Opening connection to host %s on port %d. Reading from %s.\n", hostname, port, path);
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
					fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
				} else {
					/* loop through all the results and connect to the first we can */
					for (p = servinfo; p != NULL; p = p->ai_next) {
						if ((r->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
							perror("reader: socket");
							continue;
						}
						if (connect(r->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
							close(r->sockfd);
							perror("reader: connect");
							continue;
						}
						break;
					}

					if (p == NULL) {
						fprintf(stderr, "reader: Failed to connect.\n");
					} else {
						inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
						fprintf(stderr, "reader: Connecting to %s:%d.\n", s, port);
					}
					freeaddrinfo(servinfo); /* All done with this structure */

					/* Send HTTP GET request */
					{
						char http_request[512];
						snprintf(http_request, 511, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nIcy-MetaData:0\r\n\r\n", path, hostname);
						fprintf(stderr, "reader: Sending request: %s\n", http_request);
						send(r->sockfd, http_request, strlen(http_request), 0);
					}
					/* Start reader thread... */
					if (ringbuffer_init(&(r->rb_http), HTTP_CACHE_SIZE)) {
						pthread_mutex_init(&(r->mutex), NULL);
						pthread_create(&(r->thread), NULL, http_reader_thread, r);
					} else {
						fprintf(stderr, "reader: Out of memory.\n");
					}
				}
			}
			if (hostname) free(hostname);
			if (path)     free(path);
		} else { /* Treat everything else as a local file (for now) */
			printf("reader: Opening file %s.\n", url);
			r->file = fopen(url, "r");
			r->seekable = 1;
		}
	}
	return r;
}

int reader_close(Reader *r)
{
	/* close file handle/stream... */
	if (r) {
		if (r->file) { /* local file */
			fclose(r->file);
		} else { /* http stream */
			/* close http stream */
			close(r->sockfd);
			r->eof = 1;
			pthread_join(r->thread, NULL);
		}
		if (r->buf) free(r->buf);
		if (r->intbuf) free(r->intbuf);
		free(r);
	}
	return 0;
}

int reader_is_eof(Reader *r)
{
	//printf("eof check: eof = %d r->intbuf_data_size = %d\n", r->eof, r->intbuf_data_size);
	return r->intbuf_data_size > 0 ? 0 : r->eof;
}

char reader_read_byte(Reader *r)
{
	int ch = 0;
	if (r->file) {
		ch = fgetc(r->file);
		if (ch == EOF) r->eof = 1;
	} else {
		int read_okay = 0;
		while (!read_okay) {
			char buf[1];
			pthread_mutex_lock(&(r->mutex));
			read_okay = ringbuffer_read(&(r->rb_http), buf, 1);
			pthread_mutex_unlock(&(r->mutex));
			if (!read_okay && r->eof) break;
			if (!read_okay) usleep(150);
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
			memset(r->buf, 0, r->buf_size);
			if (fread(r->buf, size, 1, r->file) < 1) {
				r->eof = feof(r->file);
				r->buf_data_size = 0; /* Maybe not such a good idea. We could have gotten a few bytes. */
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

/* Resets the stream to the beginning (if possible), returns 1 on success, 0 otherwise */
int reader_reset_stream(Reader *r)
{
	int res = 0;
	if (r->file) {
		rewind(r->file);
		res = 1;
	} else {
		/* check if stream is still in buffered part. In that case do a reset to payload start, otherwise return 0 */
		if (r->in_first_buffer) {
			/* do stream reset */
			if (r->http_payload_start)
				r->intbuf_pos = (r->http_payload_start - r->intbuf);
			res = 1;
		}
	}
	return res;
}

int reader_seek(Reader *r, int byte_offset)
{
	int res = 0;
	if (r->file) {
		fseek(r->file, byte_offset, SEEK_SET);
		res = 1;
	} else {
		/* Seeking not possible in HTTP streams */
		res = 0;
	}
	return res;
}

/*int main(int argc, char **argv)
{
	Reader *r;

	if (argc == 2) {
		r = reader_open(argv[1]);
		if (r) {
			while (!reader_is_eof(r)) {
				char *data = reader_read_bytes(r, 350);
				if (data) printf("DATA CHUNK(%d): %s\n", strlen(data), data); else printf("NO DATA\n");
				//reader_reset_stream(r);
			}
			reader_close(r);
		}
	} else {
		printf("Usage %s file/URL\n", argv[0]);
	}
	return 0;
}*/

/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wej.k.vu)
 *
 * File: httpd.c  Created: 111209
 *
 * Description: Simple (single-threaded) HTTP server (with websocket support)
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "util.h"
#include "base64.h"
#include "sha1.h"
#include "httpd.h"
#include "queue.h"
#include "core.h"
#include "json.h"
#include "websocket.h"
#include "net.h"
#include "ringbuffer.h"
#include "debug.h"
#include "dir.h"
#include "trackinfo.h"
#include "wejconfig.h"
#include "charset.h"
#include <assert.h>

#define OKAY 0
#define ERROR (-1)

#define PASSWORD_MIN_LENGTH 0
#define INDEX_FILE "gmu.html"

/*
 * 500 Internal Server Error
 * 501 Not Implemented
 * 400 Bad Request
 * 505 HTTP Version not supported
 */

static Queue queue;
static char  webserver_root[256];

static const char *http_status[] = {
	"100", "Continue",
	"101", "Switching Protocols",
	"200", "OK",
	"400", "Bad Request",
	"404", "Not Found",
	"403", "Forbidden",
	"417", "Expectation Failed",
	"500", "Internal Server Error",
	"501", "Not Implemented",
	"505", "HTTP Version not supported",
	NULL
};

static const char *mime_type[] = {
	"jpg",  "image/jpeg",
	"jpeg", "image/jpeg",
	"png",  "image/png",
	"htm",  "text/html",
	"html", "text/html",
	"css",  "text/css",
	"js",   "application/x-javascript",
	"ico",  "image/vnd.microsoft.icon",
	"gz",   "application/x-gzip",
	"zip",  "application/zip",
	"txt",  "text/plain",
	NULL
};

static const char *get_mime_type(const char *url)
{
	const char *res = "text/html";
	const char *ext = url ? strrchr(url, '.') : NULL;
	if (ext) {
		size_t i;
		for (i = 0; mime_type[i]; i+=2) {
			if (strcmp(ext+1, mime_type[i]) == 0) {
				res = mime_type[i+1];
				break;
			}
		}
	}
	return res;
}

/* Returns pointer to next char after value */
static const char *get_next_key_value_pair(
	const char *str,
	char       *key,
	size_t      key_len,
	char       *value,
	size_t      value_len
)
{
	size_t sc = 0, i = 0;
	int    eoh = 0;

	/* Check for end of header: "\n\r\n\r" */
	if ((str[sc] == '\r' && str[sc+1] == '\n') || str[sc] == '\n') {
		eoh = 1;
		wdprintf(V_DEBUG, "httpd", "End of header detected!\n");
	}

	key[0] = '\0';
	value[0] = '\0';
	if (!eoh) {
		/* extract key */
		char ch = str[0];
		while (ch != '\r' && ch != '\n' && ch != ':' && ch != '\0') {
			ch = str[sc++];
			if (i < key_len-1 && ch != ':' && ch != '=' && ch != '\r' && ch != '\n') key[i++] = ch;
		}
		key[i] = '\0';

		/* extract value */
		i = 0;
		while (str[sc] == ' ' || str[sc] == '\t') sc++; /* skip spaces after ":" */
		ch = ' ';
		while (ch != '\r' && ch != '\n' && ch != '\0') {
			ch = str[sc++];
			if (i < value_len-1 && ch != '\r' && ch != '\n') value[i++] = ch;
		}
		value[i] = '\0';
	}
	return str+sc+1;
}

static int server_running = 0;

static int websocket_send_string(Connection *c, const char *str)
{
	int res = 0;
	if (str && c->fd) {
		res = websocket_send_str(c->fd, str, 0);
		if (res)
			connection_reset_timeout(c);
		else
			c->state = CON_ERROR;
	}
	return res;
}

/**
 * Initializes a new connection with a given file descriptor fd and
 * attaches the connection before the connection supplied with 'prev'
 * (if prev is not NULL), so that the new connection appears before 
 * prev and prev's predecessor (if there is one), is then the
 * predecessor of the new connection.
 * A reference to the new connection is returned, or NULL on failure.
 */
Connection *connection_init(int fd, const char *client_ip, Connection *prev)
{
	Connection *c = calloc(1, sizeof(Connection));
	if (c) {
		int res = 0;
		c->connection_time = time(NULL);
		c->state = CON_HTTP_NEW;
		c->fd = fd;
		c->http_request_header = NULL;
		c->authentication_okay = 0;
		c->next = prev ? prev : NULL;
		c->prev = prev ? prev->prev : NULL;
		if (client_ip) {
			strncpy(c->client_ip, client_ip, INET6_ADDRSTRLEN);
			res = ringbuffer_init(&(c->rb_receive), HTTP_RINGBUFFER_BUFFER_SIZE);
		}
		if (!res) {
			c->state = CON_ERROR;
		} else if (prev) {
			prev->prev = c;
		}
	}
	return c;
}

void connection_reset_timeout(Connection *c)
{
	c->connection_time = time(NULL);
}

int connection_is_valid(Connection *c)
{
	return (c->connection_time > 0);
}

void connection_close(Connection *c)
{
	if (c->fd) {
		int cres = close(c->fd);
		wdprintf(
			V_DEBUG,
			"httpd",
			"Closing connection %d: %s\n",
			c->fd,
			cres == 0 ? "ok" : strerror(errno)
		);
	}
	if (c->http_request_header) {
		free(c->http_request_header);
		c->http_request_header = NULL;
	}
	ringbuffer_free(&(c->rb_receive));
	if (c->prev) c->prev->next = c->next;
	if (c->next) c->next->prev = c->prev;
	free(c);
}

void connection_free_request_header(Connection *c)
{
	if (c->http_request_header) {
		free(c->http_request_header);
		c->http_request_header = NULL;
	}
}

int connection_is_timed_out(Connection *c)
{
	int timeout = c->state == CON_WEBSOCKET_OPEN ? 
	              CONNECTION_TIMEOUT_WEBSOCKET : CONNECTION_TIMEOUT_HTTP;
	return (time(NULL) - c->connection_time > timeout);
}

void connection_set_state(Connection *c, ConnectionState s)
{
	wdprintf(V_DEBUG, "httpd", "Connection: new state=%d\n", s);
	c->state = s;
}

ConnectionState connection_get_state(Connection *c)
{
	return c->state;
}

int connection_is_authenticated(Connection *c)
{
	return c->authentication_okay;
}

int connection_file_is_open(Connection *c)
{
	return (c->local_file ? 1 : 0);
}

int connection_file_open(Connection *c, const char *filename)
{
	struct stat fst;
	if (c->local_file) fclose(c->local_file);
	c->total_size = 0;
	c->local_file = NULL;
	if (stat(filename, &fst) == 0) {
		if (S_ISREG(fst.st_mode)) {
			wdprintf(
				V_DEBUG,
				"httpd", "Connection: Opening file %s (%zd bytes)...\n",
				filename,
				fst.st_size
			);
			c->local_file = fopen(filename, "r");
			c->total_size = fst.st_size;
		}
	}
	c->remaining_bytes_to_send = c->total_size;
	return (c->local_file ? 1 : 0);
}

void connection_file_close(Connection *c)
{
	if (c->local_file) {
		fclose(c->local_file);
		c->total_size = 0;
		c->local_file = NULL;
	}
}

int connection_get_number_of_bytes_to_send(Connection *c)
{
	return c->remaining_bytes_to_send;
}

int connection_file_read_chunk(Connection *c)
{
	if (c->local_file) {
		char blob[CHUNK_SIZE];
		if (c->fd && c->local_file && c->remaining_bytes_to_send > 0) {
			size_t size = CHUNK_SIZE;
			size_t actual_size;

			if (c->remaining_bytes_to_send < CHUNK_SIZE) size = c->remaining_bytes_to_send;
			wdprintf(V_DEBUG, "httpd", "Connection %d: Reading chunk of data: %d bytes\n", c->fd, size);
			actual_size = fread(blob, 1, size, c->local_file);
			if (actual_size > 0) {
				if (!net_send_block(c->fd, (unsigned char *)blob, actual_size))
					connection_set_state(c, CON_ERROR);
				c->remaining_bytes_to_send -= actual_size;
			} else { /* Reading from file returned no data */
				c->remaining_bytes_to_send = 0;
			}
			c->connection_time = time(NULL);
		} else {
			connection_set_state(c, CON_HTTP_IDLE);
			if (c->local_file) fclose(c->local_file);
			c->local_file = NULL;
		}
		/* If eof, set connection state to HTTP_IDLE and close file, 
		 * set local_file to NULL */
		if (c->local_file && feof(c->local_file)) {
			fclose(c->local_file);
			c->local_file = NULL;
			connection_set_state(c, CON_HTTP_IDLE);
		}
	} else {
		wdprintf(V_DEBUG, "httpd", "Connection: ERROR, file handle invalid.\n");
	}
	return 0;
}

/* Returns true if connection originates from localhost */
int connection_is_local(Connection *c)
{
	return (strcmp(c->client_ip, "127.0.0.1") == 0);
}

int connection_authenticate(Connection *c, const char *password)
{
	int         no_local_password_required = 0;
	ConfigFile *cf = gmu_core_get_config();
	const char *password_ref;

	gmu_core_config_acquire_lock();
	password_ref = cf ? cfg_get_key_value(cf, "gmuhttp.Password") : NULL;
	no_local_password_required = cfg_get_boolean_value(cf, "gmuhttp.DisableLocalPassword");

	c->authentication_okay = 0;
	if (password && password_ref &&
	    strlen(password) >= PASSWORD_MIN_LENGTH &&
	    strcmp(password, password_ref) == 0) {
		c->authentication_okay = 1;
	} else if (no_local_password_required && connection_is_local(c)) {
		wdprintf(V_DEBUG, "httpd", "Accepting local and password-less client connection.\n");
		c->authentication_okay = 1;
	}
	gmu_core_config_release_lock();

	if (c->authentication_okay) {
		websocket_send_string(c, "{\"cmd\":\"login\",\"res\":\"success\"}");
	} else {
		websocket_send_string(c, "{\"cmd\":\"login\",\"res\":\"failure\"}");
	}
	return c->authentication_okay;
}

static void send_http_header(
	int         soc,
	const char *code,
	size_t      length,
	time_t     *time_modified,
	const char *content_type
)
{
	char        msg[255] = {0};
	struct tm  *ptm = NULL;
	time_t      stime;
	const char *code_text = "";
	size_t      i;

	for (i = 0; http_status[i]; i += 2) {
		if (strcmp(http_status[i], code) == 0) {
			code_text = http_status[i+1];
			break;
		}
	}
	snprintf(msg, 254, "HTTP/1.1 %s %s\r\n", code, code_text);
	net_send_buf(soc, msg);
	stime = time(NULL);
	ptm = gmtime(&stime);
	strftime(msg, 255, "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", ptm);
	net_send_buf(soc, msg);
	net_send_buf(soc, "Server: Gmu http server\r\n");

	if (time_modified != NULL) {
		struct tm *tm_modified = gmtime(time_modified);
		strftime(msg, 255, "Last-Modified: %a, %d %b %Y %H:%M:%S %Z\r\n", tm_modified);
		net_send_buf(soc, msg);
	}

	net_send_buf(soc, "Accept-Ranges: none\r\n");
	snprintf(msg, 254, "Content-Length: %zd\r\n", length);
	net_send_buf(soc, msg);
	/*send_buf(soc, "Connection: close\r\n");*/
	snprintf(msg, 254, "Content-Type: %s\r\n", content_type);
	net_send_buf(soc, msg);
	net_send_buf(soc, "\r\n");
}

static void webserver_main_loop(int listen_fd);
static int  tcp_server_init(unsigned port, int local_only);

void *httpd_run_server(void *data)
{
	unsigned           port = SERVER_PORT;
	int                local_only = 1, fd, errsv;
	HTTPD_Init_Params *ip = (HTTPD_Init_Params *)data;

	if (ip && ip->webserver_root)
		strncpy(webserver_root, ip->webserver_root, 255);
	else
		webserver_root[0] = '\0';
	if (ip)
		local_only = ip->local_only;
	else
		wdprintf(V_WARNING, "httpd", "Warning: Init params missing!\n");
	queue_init(&queue);
	wdprintf(V_INFO, "httpd", "Starting server on port %d.\n", port);
	wdprintf(V_INFO, "httpd", "Listening on %s.\n",
	         local_only ? "LOCAL interface only" : "ALL available interfaces");
	wdprintf(V_INFO, "httpd", "Webserver root directory: %s\n", webserver_root);
	if (ip) free(ip);
	server_running = 1;
	do {
		fd = tcp_server_init(port, local_only);
		errsv = errno;
		if (fd != ERROR) {
			webserver_main_loop(fd);
		} else {
			wdprintf(V_WARNING, "httpd", "Unable to listen on port %d: %s\n", port, strerror(errno));
			if (errsv == EADDRINUSE) usleep(1000000); /* Wait a second before retrying */
		}
	} while (server_running && fd == ERROR && errsv == EADDRINUSE);
	wdprintf(V_DEBUG, "httpd", "Shutdown.\n");
	return NULL;
}

#define MAXLEN 4096

/**
 * Open server listen port 'port'
 * Returns socket fd or ERROR
 */
static int tcp_server_init(unsigned port, int local_only)
{
	int listen_fd, ret, yes = 1;

	listen_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (listen_fd >= 0) {
		/* Avoid "Address already in use" error: */
		ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (ret == 0) {
			char   port_str[6];
			struct addrinfo hints, *res;

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;  /* use IPv4 or IPv6, whichever */
			hints.ai_socktype = SOCK_STREAM;
			if (!local_only) hints.ai_flags = AI_PASSIVE;  /* fill in my IP for me */
			snprintf(port_str, 6, "%d", port);
			ret = getaddrinfo(local_only ? "127.0.0.1" : NULL, port_str, &hints, &res);
			if (ret != 0) {
				wdprintf(V_WARNING, "httpd", "WARNING: %s\n", gai_strerror(ret));
			} else {
				struct addrinfo *r;
				for (r = res; r != NULL; r = r->ai_next) {
					listen_fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
					if (listen_fd != -1) {
						ret = bind(listen_fd, r->ai_addr, r->ai_addrlen);
						break;
					} else {
						ret = -1;
					}
				}
				freeaddrinfo(res);
			}

			if (ret == 0) {
				ret = listen(listen_fd, 5);
				if (ret < 0) listen_fd = ERROR;
			} else {
				listen_fd = ERROR;
			}
		} else {
			listen_fd = ERROR;
		}
	} else {
		listen_fd = ERROR;
	}
	return listen_fd;
}

/**
 * Open connection to client. To be called for every new client.
 * Sets conn.state == CON_ERROR in result in case of an error.
 */
static Connection tcp_server_client_init(int listen_fd)
{
	struct sockaddr_storage sock;
	socklen_t               socklen;
	Connection              conn;

	socklen = sizeof(sock);
	memset(&sock, 0, socklen);
	conn.fd = accept(listen_fd, (struct sockaddr *)&sock, &socklen);
	conn.state = (conn.fd < 0 ? CON_ERROR : CON_HTTP_NEW);
	if (conn.state == CON_HTTP_NEW) {
		if (sock.ss_family == AF_INET || sock.ss_family == AF_INET6) {
			unsigned ipver = sock.ss_family == AF_INET ? 4 : 6;
			if (!inet_ntop(
				sock.ss_family,
				sock.ss_family == AF_INET ?
					(const void *)&((struct sockaddr_in *)&sock)->sin_addr :
					(const void *)&((struct sockaddr_in6 *)&sock)->sin6_addr,
				conn.client_ip, INET6_ADDRSTRLEN
			)) {
				conn.state = CON_ERROR;
			} else {
				wdprintf(
					V_DEBUG,
					"httpd", "Incoming IPv%d connection from %s...\n",
					ipver,
					conn.client_ip
				);
			}
		} else {
			conn.state = CON_ERROR;
			wdprintf(V_DEBUG, "httpd", "Unsupported address family.\n");
		}
	} else {
		wdprintf(V_DEBUG, "httpd", "ERROR: Problem with incoming connection.\n");
	}
	return conn;
}

/**
 * Write to client socket
 * Returns OKAY when the message could be written completely, ERROR otherwise
 */
static int tcp_server_write(int fd, const char buf[], size_t buflen)
{
	return write(fd, buf, buflen) == buflen ? OKAY : ERROR;
}

/**
 * Read from client socket
 * Returns OKAY when reading was successful, ERROR otherwise
 */
static int tcp_server_read(int fd, char buf[], ssize_t *buflen)
{
	int res = OKAY;
	*buflen = read(fd, buf, *buflen);
	if (*buflen <= 0) { /* End of TCP connection */
		res = ERROR; /* Connection fd should be closed */
	}
	return res;
}

static int is_valid_resource(const char *str)
{
	int    res = 1;
	size_t i, len = 0;

	/* Resource strings MUST NOT contain substrings containing ".." */
	if (str) {
		len = strlen(str);
		for (i = 0; i < len; i++) {
			if (str[i] == '.') {
				if (i+1 < len && str[i+1] == '.') {
					res = 0;
					break;
				}
			}
		}
	}
	return res;
}

static HTTPCommand get_command(const char *str)
{
	HTTPCommand cmd = UNKNOWN;
	if (str) {
		if      (strcmp(str, "GET")  == 0) cmd = GET;
		else if (strcmp(str, "HEAD") == 0) cmd = HEAD;
		else if (strcmp(str, "POST") == 0) cmd = POST;
	}
	return cmd;
}

static void http_response_bad_request(int fd, int head_only)
{
	const char *str = "<h1>400 Bad Request</h1>";
	size_t      body_len = strlen(str);
	send_http_header(fd, "400", body_len, NULL, "text/html");
	if (!head_only) tcp_server_write(fd, str, body_len);
}

static void http_response_not_found(int fd, int head_only)
{
	const char *str = "<h1>404 File not found</h1>";
	size_t      body_len = strlen(str);
	send_http_header(fd, "404", body_len, NULL, "text/html");
	if (!head_only) tcp_server_write(fd, str, body_len);
}

static void http_response_not_implemented(int fd)
{
		const char *str =
			"<html><head><title>501 Not implemented</title></head>\n"
			"<body>\n<h1>501 Not implemented</h1>\n<p>Invalid method.</p>\n"
			"<p><hr /><i>Gmu http server</i></p>\n</body>\n"
			"</html>\r\n\r\n";
		size_t      body_len = strlen(str);
		send_http_header(fd, "501", body_len, NULL, "text/html");
		tcp_server_write(fd, str, body_len);
}

static int process_command(int rfd, Connection *c)
{
	const char *command = NULL, *resource = NULL;
	const char *http_version = NULL, *options = NULL;
	char       *host = NULL, websocket_key[32] = "";

	if (c->http_request_header) {
		size_t len = strlen(c->http_request_header);
		size_t i;
		int    state = 1;
		char  *str;

		str = malloc(len+1);
		if (str) {
			strncpy(str, c->http_request_header, len);
			str[len] = '\0';
			command = str; /* GET, POST, HEAD ... */

			/* Parse HTTP request (first line)... */
			for (i = 0; i < len && str[i] != '\r' && str[i] != '\n'; i++) {
				if (str[i] == ' ' || str[i] == '\t') {
					str[i] = '\0'; i++;
					for (; str[i] == ' ' || str[i] == '\t'; i++);
					switch (state) {
						case 1: /* resource */
							resource = str+i;
							break;
						case 2: /* http version */
							http_version = str+i; /* e.g. "HTTP/1.1" */
							break;
					}
					state++;
				}
			}
			str[i] = '\0';
			/* Parse options (following lines)... */
			if (i < len) {
				options = str+i+1;
				if (options) {
					len = strlen(options);
					for (; options[0] != '\0' && (options[0] == '\r' || options[0] == '\n'); options++);
				} else {
					wdprintf(V_DEBUG, "httpd", "No options. :(\n");
				}
			} else {
				wdprintf(V_DEBUG, "httpd", "Unexpected end of data.\n");
			}
			for (i = len-1; i > 0 && (str[i] == '\n' || str[i] == '\r'); i--)
				str[i] = '\0';
		}
		if (command) wdprintf(V_DEBUG, "httpd", "%04d Command: [%s]\n", rfd, command);
		if (resource) {
			wdprintf(V_DEBUG, "httpd", "%04d Resource: [%s]\n", rfd, resource);
			wdprintf(V_DEBUG, "httpd", "%04d Mime type: %s\n", rfd, get_mime_type(resource));
		}
		if (http_version) wdprintf(V_DEBUG, "httpd", "%04d http_version: [%s]\n", rfd, http_version);
		if (options) {
			char        key[128], value[256];
			const char *opts = options;
			int         websocket_version = 0, websocket_upgrade = 0, websocket_connection = 0;

			wdprintf(V_DEBUG, "httpd", "%04d Options: [%s]\n", rfd, options);
			while (opts) {
				opts = get_next_key_value_pair(opts, key, 128, value, 256);
				if (key[0]) {
					wdprintf(V_DEBUG, "httpd", "key=[%s] value=[%s]\n", key, value);
					if (strcasecmp(key, "Host") == 0 && value[0]) {
						int len = strlen(value);
						if (host) free(host); /* Just in case we more than one Host key */
						host = malloc(len+1);
						if (host) memcpy(host, value, len+1);
					}
					if (strcasecmp(key, "Upgrade") == 0 && strcasecmp(value, "websocket") == 0)
						websocket_upgrade = 1;
					if (strcasecmp(key, "Connection") == 0 && strcasestr(value, "Upgrade"))
						websocket_connection = 1;
					if (strcasecmp(key, "Sec-WebSocket-Key") == 0) {
						strncpy(websocket_key, value, 31);
						websocket_key[31] = '\0';
					}
					if (strcasecmp(key, "Sec-WebSocket-Version") == 0 && value[0]) 
						websocket_version = atoi(value);
				}
				if (!(key[0])) break;
			}
			if (websocket_upgrade && websocket_connection && websocket_version && websocket_key[0] &&
			    get_command(command) == GET) {
				wdprintf(V_DEBUG, "httpd", "---> Client wants to upgrade connection to WebSocket version %d with key '%s'.\n",
				       websocket_version, websocket_key);
			} else { /* If there is no valid websocket request, clear the key as it being used later
			            to check for websocket request */
				websocket_key[0] = '\0';
			}
		}

		if (is_valid_resource(resource)) {
			int head_only = 0;
			switch (get_command(command)) {
				case HEAD:
					head_only = 1;
				case GET: {
					int file_okay = 0;
					if (host) { /* If no Host has been supplied, the query is invalid, thus repond with 400 */
						if (websocket_key[0]) { /* Websocket connection upgrade */
							const char *websocket_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
							char        str[128], str2[256];
							SHA1_CTX    sha;
							uint8_t     digest[SHA1_DIGEST_SIZE];
							const char *hellostr;
							ConfigFile *cf = gmu_core_get_config();

							wdprintf(V_DEBUG, "httpd", "%04d Proceeding with websocket connection upgrade...\n", rfd);
							/* 1) Calculate response key (SHA1, Base64) */
							snprintf(str, 61, "%s%s", websocket_key, websocket_magic);
							str[60] = '\0';
							wdprintf(V_DEBUG, "httpd", "Voodoo = '%s'\n", str);
							SHA1_Init(&sha);
							SHA1_Update(&sha, (uint8_t *)str, strlen(str));
							SHA1_Final(&sha, digest);
							memset(str, 0, 127);
							base64_encode_data((unsigned char *)digest, 20, str, 30);
							wdprintf(V_DEBUG, "httpd", "Calculated base 64 response value: '%s'\n", str);
							/* 2) Send 101 response with appropriate data */
							net_send_buf(rfd, "HTTP/1.1 101 Switching Protocols\r\n");
							net_send_buf(rfd, "Server: Gmu http server\r\n");
							net_send_buf(rfd, "Upgrade: websocket\r\n");
							net_send_buf(rfd, "Connection: Upgrade\r\n");
							snprintf(str2, 255, "Sec-WebSocket-Accept: %s\r\n", str);
							wdprintf(V_DEBUG, "httpd", str2);
							net_send_buf(rfd, str2);
							net_send_buf(rfd, "\r\n");
							/* 3) Set flags in connection struct to WebSocket */
							connection_set_state(c, CON_WEBSOCKET_OPEN);
							gmu_core_config_acquire_lock();
							hellostr = cfg_get_boolean_value(cf, "gmuhttp.DisableLocalPassword") ?
								"{ \"cmd\": \"hello\", \"need_password\": \"no\" }" :
								"{ \"cmd\": \"hello\", \"need_password\": \"yes\" }";
							gmu_core_config_release_lock();
							websocket_send_string(c, hellostr);
						} else if (!connection_file_is_open(c)) { /* ?? open file (if not open already) ?? */
							char filename[512] = {0};

							snprintf(filename, 511, "%s/htdocs%s", webserver_root, resource);
							file_okay = connection_file_open(c, filename);
							if (!file_okay) {
								snprintf(filename, 511, "%s/htdocs%s/%s", webserver_root, resource, INDEX_FILE);
								file_okay = connection_file_open(c, filename);
							}
							if (file_okay) {
								int         time_ok = 0;
								struct stat st;

								if (stat(filename, &st) == 0) time_ok = 1;
								if (!head_only) connection_set_state(c, CON_HTTP_BUSY);
								send_http_header(
									rfd,
									"200",
									connection_get_number_of_bytes_to_send(c),
									time_ok ? &(st.st_ctime) : NULL,
									get_mime_type(resource)
								);
								if (head_only) connection_file_close(c);
							} else { /* 404 */
								http_response_not_found(rfd, head_only);
							}
						}
					} else {
						http_response_bad_request(rfd, head_only);
					}
					break;
				}
				case POST:
					http_response_not_implemented(rfd);
					break;
				default:
					http_response_bad_request(rfd, 0);
					break;
			}
		} else { /* Invalid resource string */
			http_response_not_found(rfd, 0);
		}

		if (host) free(host);
		if (str) free(str);
		connection_free_request_header(c);
	}
	return 0;
}

/*
 * Send string "str" to all connected WebSocket clients
 */
void httpd_send_websocket_broadcast(const char *str)
{
	queue_push(&queue, str);
}

#define MSG_MAX_LEN 1024

void gmu_http_playlist_get_info(Connection *c)
{
	char msg[MSG_MAX_LEN];
	int  r = snprintf(
		msg,
		MSG_MAX_LEN,
		"{ \"cmd\": \"playlist_info\", \"changed_at_position\" : 0, \"length\" : %zd }",
		gmu_core_playlist_get_length()
	);
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
}

void gmu_http_playlist_get_item(int id, Connection *c)
{
	char   msg[MSG_MAX_LEN], *tmp_title = NULL;
	Entry *item;
	int    r;

	gmu_core_playlist_acquire_lock();
	item = gmu_core_playlist_get_entry(id);
	tmp_title = json_string_escape_alloc(gmu_core_playlist_get_entry_name(item));
	gmu_core_playlist_release_lock();
	r = snprintf(
		msg,
		MSG_MAX_LEN,
		"{ \"cmd\": \"playlist_item\", \"position\" : %d, \"title\": \"%s\", \"length\": %d }",
		id,
		tmp_title ? tmp_title : "??",
		0
	);
	if (tmp_title) free(tmp_title);
	if (r > 0 && !charset_is_valid_utf8_string(msg)) {
		r = snprintf(
			msg,
			MSG_MAX_LEN,
			"{ \"cmd\": \"playlist_item\", \"position\" : %d, \"title\": \"(Invalid UTF-8)\", \"length\": %d }",
			id,
			0
		);
	}
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
}

void gmu_http_get_current_trackinfo(Connection *c)
{
	int        r = 0;
	char       msg[MSG_MAX_LEN];
	TrackInfo *ti = gmu_core_get_current_trackinfo_ref();

	if (trackinfo_acquire_lock(ti)) {
		char *ti_artist = json_string_escape_alloc(trackinfo_get_artist(ti));
		char *ti_title  = json_string_escape_alloc(trackinfo_get_title(ti));
		char *ti_album  = json_string_escape_alloc(trackinfo_get_album(ti));
		char *ti_date   = json_string_escape_alloc(trackinfo_get_date(ti));
		r = snprintf(
			msg,
			MSG_MAX_LEN,
			"{ \"cmd\": \"trackinfo\", \"artist\": \"%s\", \"title\": \"%s\", " \
			"\"album\": \"%s\", \"date\": \"%s\", " \
			"\"length_min\": %d, \"length_sec\": %d, \"pl_pos\": %d  }",
			ti_artist ? ti_artist : "",
			ti_title  ? ti_title  : "",
			ti_album  ? ti_album  : "",
			ti_date   ? ti_date   : "",
			trackinfo_get_length_minutes(ti),
			trackinfo_get_length_seconds(ti),
			0
		);
		if (ti_artist) free(ti_artist);
		if (ti_title)  free(ti_title);
		if (ti_album)  free(ti_album);
		if (ti_date)   free(ti_date);
		if (r > 0 && !charset_is_valid_utf8_string(msg)) {
			snprintf(
				msg,
				MSG_MAX_LEN,
				"{ \"cmd\": \"trackinfo\", \"artist\": \"(Invalid UTF-8)\", " \
				"\"title\": \"(Invalid UTF-8)\", \"album\": \"(Invalid UTF-8)\", " \
				"\"date\": \"(Invalid UTF-8)\", " \
				"\"length_min\": %d, \"length_sec\": %d, \"pl_pos\": %d  }",
				trackinfo_get_length_minutes(ti),
				trackinfo_get_length_seconds(ti),
				0
			);
		}
		trackinfo_release_lock(ti);
	}
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
}

void gmu_http_playmode_get_info(Connection *c)
{
	char msg[MSG_MAX_LEN];
	int  r = snprintf(msg, MSG_MAX_LEN,
	                  "{ \"cmd\": \"playmode_info\", \"mode\" : %d }",
	                  gmu_core_playlist_get_play_mode());
	if (r < MSG_MAX_LEN && r > 0) httpd_send_websocket_broadcast(msg);
}

void gmu_http_send_initial_information(Connection *c)
{
	char msg[MSG_MAX_LEN];
	int  r = snprintf(
		msg,
		MSG_MAX_LEN,
		"{ \"cmd\": \"playlist_change\", \"changed_at_position\" : 0, \"length\" : %zd }",
		gmu_core_playlist_get_length()
	);
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
	r = snprintf(
		msg,
		MSG_MAX_LEN,
		"{ \"cmd\": \"playback_state\", \"state\" : %d }",
		gmu_core_get_status()
	);
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
	r = snprintf(
		msg,
		MSG_MAX_LEN,
		"{ \"cmd\": \"volume_info\", \"volume\" : %d }",
		gmu_core_get_volume()
	);
	if (r < MSG_MAX_LEN && r > 0) httpd_send_websocket_broadcast(msg);
}

#define MAX_LEN 32768

static void gmu_http_read_dir(const char *directory, Connection *c)
{
	Dir        *dir;
	char        base_dir[256];
	const char *tmp;
	ConfigFile *cf = gmu_core_get_config();

	dir = dir_init();
	gmu_core_config_acquire_lock();
	tmp = cfg_get_key_value(cf, "gmuhttp.BaseDir");
	if (tmp && strlen(tmp) < 256) {
		strcpy(base_dir, tmp);
	} else {
		strcpy(base_dir, "/");
	}
	gmu_core_config_release_lock();
	if (strncmp(base_dir, directory, strlen(base_dir)) != 0)
		directory = base_dir;
	dir_set_base_dir(dir, base_dir);
	dir_set_ext_filter(dir, gmu_core_get_file_extensions(), 1);

	if (dir_read(dir, directory, 1)) {
		size_t i = 0;
		int    num_files = dir_get_number_of_files(dir);
		while (i < num_files) {
			char   res[MAX_LEN], *jpath = NULL, *spath;
			size_t pos;

			spath = dir_get_new_dir_alloc("/", directory);
			if (spath) {
				jpath = json_string_escape_alloc(spath);
				free(spath);
			}
			if (jpath) {
				snprintf(
					res,
					MAX_LEN,
					"{ \"cmd\": \"dir_read\", \"res\": \"ok\", \"path\": \"%s\", \"data\": {",
					jpath
				);
				free(jpath);
				for (pos = strlen(res); i < num_files; i++) {
					char  *tmp = json_string_escape_alloc(dir_get_filename(dir, i));
					long   filesize = dir_get_filesize(dir, i);
					size_t pos_prev = pos;

					if (tmp) {
						snprintf(
							res+pos,
							MAX_LEN-pos,
							"\"%zd\": { \"name\": \"%s\", \"size\": %ld, \"is_dir\": %d },",
							i,
							tmp,
							filesize,
							dir_get_flag(dir, i) == DIRECTORY
						);
						free(tmp);
					}
					pos = strlen(res);
					if (pos > MAX_LEN - 65) {
						pos = pos_prev;
						break;
					}
				}
				snprintf(res+pos, MAX_LEN-pos, "\"-1\": \"END\" } }");
				wdprintf(V_DEBUG, "httpd", "dir_read result: [%s] size=%d\n", res, pos);
				if (!charset_is_valid_utf8_string(res))
					wdprintf(V_DEBUG, "httpd", "Invalid UTF-8 found!!\n", res);
				else if (!websocket_send_string(c, res))
					wdprintf(V_DEBUG, "httpd", "Sending websocket message failed! :(\n");
			} else {
				websocket_send_string(c, "{ \"cmd\": \"dir_read\", \"res\" : \"error\", \"msg\" : \"Unable to read directory\" }");
			}
		}
	} else { /* Error condition */
		websocket_send_string(c, "{ \"cmd\": \"dir_read\", \"res\" : \"error\", \"msg\" : \"Unable to read directory\" }");
	}
	dir_free(dir);
}

static void gmu_http_ping(Connection *c)
{
	websocket_send_string(c, "{\"cmd\":\"pong\"}");
}

static void gmu_http_medialib_search(Connection *c, const char *type, const char *str)
{
	TrackInfo ti;
	size_t    i = 0;
	char      rstr[1024];
	int       res = gmu_core_medialib_search_find(GMU_MLIB_ANY, str);

	websocket_send_string(c, "{ \"cmd\": \"mlib_search_start\" }");

	if (res) {
		for (ti = gmu_core_medialib_search_fetch_next_result();
			 ti.id >= 0;
			 ti = gmu_core_medialib_search_fetch_next_result(), i++) {
			char *artist = json_string_escape_alloc(ti.artist);
			char *title  = json_string_escape_alloc(ti.title);
			char *album  = json_string_escape_alloc(ti.album);
			char *date   = json_string_escape_alloc(ti.date);
			char *file   = json_string_escape_alloc(ti.file_name);
			snprintf(
				rstr,
				1023,
				"{\"cmd\":\"mlib_result\", \"pos\":%zd,\"id\":%d,\"artist\":\"%s\",\"title\":\"%s\",\"album\":\"%s\",\"date\":\"%s\",\"file\":\"%s\"}",
				i,
				ti.id,
				artist,
				title,
				album,
				date,
				file
			);
			free(artist);
			free(title);
			free(album);
			free(date);
			free(file);
			websocket_send_string(c, rstr);
		}
	}
	gmu_core_medialib_search_finish();
	websocket_send_string(c, "{ \"cmd\": \"mlib_search_done\" }");
}

static void gmu_http_medialib_browse_artists(Connection *c)
{
	const char  *str = NULL;
	size_t       i = 0;
	char         rstr[1024];
	int          res = gmu_core_medialib_browse_artists();

	if (res) {
		for (str = gmu_core_medialib_browse_fetch_next_result();
			 str;
			 str = gmu_core_medialib_browse_fetch_next_result(), i++) {
			char *artist = json_string_escape_alloc(str);
			snprintf(
				rstr,
				1023,
				"{\"cmd\":\"mlib_browse_result\", \"pos\":%zd,\"artist\":\"%s\"}",
				i,
				artist
			);
			free(artist);
			websocket_send_string(c, rstr);
		}
	}
	gmu_core_medialib_browse_finish();
}

/**
 * Returns 1 if connection is still open and usable or 0 if the
 * connection must be considered dead and needs to be closed
 */
static int gmu_http_handle_websocket_message(const char *message, Connection *c)
{
	JSON_Object *json = json_parse_alloc(message);
	int          res = 1;
	if (json && !json_object_has_parse_error(json)) { /* Valid JSON data received */
		/* Analyze command in JSON data */
		const char *cmd = json_get_string_value_for_key(json, "cmd");
		if (cmd && strcmp(cmd, "login") == 0) {
			char *password = json_get_string_value_for_key(json, "password");
			if (!connection_authenticate(c, password)) {
				wdprintf(V_DEBUG, "httpd", "Authentication failed.\n");
				res = 0;
			} else {
				gmu_http_send_initial_information(c);
			}
		} else if (cmd && connection_is_authenticated(c)) {
			wdprintf(V_DEBUG, "httpd", "Got command (via JSON data): '%s'\n", cmd);
			if (strcmp(cmd, "play") == 0) {
				JSON_Key_Type type   = json_get_type_for_key(json, "item");
				int           item   = json_get_integer_value_for_key(json, "item");
				/* source: 'playlist' (default), 'medialib' or 'file' */
				const char   *source = json_get_string_value_for_key(json, "source");
				const char   *file = json_get_string_value_for_key(json, "file");

				if ((type == JSON_NUMBER && item >= 0) || file) {
					if (source && strcmp(source, "medialib") == 0) {
						gmu_core_play_medialib_item(item);
					} else if ((source && strcmp(source, "file") == 0) || file) {
						/* TODO: Check if path is allowed before playing the file */
						gmu_core_play_file(file);
					} else {
						gmu_core_play_pl_item(item);
					}
				} else {
					gmu_core_play();
				}
			} else if (strcmp(cmd, "pause") == 0) {
				gmu_core_pause();
			} else if (strcmp(cmd, "play_pause") == 0) {
				gmu_core_play_pause();
			} else if (strcmp(cmd, "next") == 0) {
				gmu_core_next();
			} else if (strcmp(cmd, "prev") == 0) {
				gmu_core_previous();
			} else if (strcmp(cmd, "stop") == 0) {
				gmu_core_stop();
			} else if (strcmp(cmd, "trackinfo") == 0) {
				gmu_http_get_current_trackinfo(c);
			} else if (strcmp(cmd, "playlist_get_item") == 0) {
				int item = (int)json_get_number_value_for_key(json, "item");
				if (item >= 0) {
					wdprintf(V_DEBUG, "httpd", "item=%d\n", item);
					gmu_http_playlist_get_item(item, c);
				}
			} else if (strcmp(cmd, "playlist_get_info") == 0) {
				gmu_http_playlist_get_info(c);
			} else if (strcmp(cmd, "dir_read") == 0) {
				const char *dir = json_get_string_value_for_key(json, "dir");
				if (dir) {
					gmu_http_read_dir(dir, c);
				}
			} else if (strcmp(cmd, "get_current_track") == 0) {
			} else if (strcmp(cmd, "playlist_add") == 0) {
				const char *path = json_get_string_value_for_key(json, "path");
				const char *type = json_get_string_value_for_key(json, "type");
				if (path) {
					if (type && strcmp(type, "file") == 0) {
						gmu_core_playlist_add_file(path);
					} else {
						gmu_core_playlist_add_dir(path);
					}
				}
			} else if (strcmp(cmd, "playlist_clear") == 0) {
				gmu_core_playlist_clear();
			} else if (strcmp(cmd, "playlist_playmode_cycle") == 0) {
				gmu_core_playlist_cycle_play_mode();
			} else if (strcmp(cmd, "playlist_playmode_set") == 0) {
				int pm = (int)json_get_number_value_for_key(json, "mode");
				gmu_core_playlist_set_play_mode(pm);
			} else if (strcmp(cmd, "playlist_playmode_get_info") == 0) {
				gmu_http_playmode_get_info(c);
			} else if (strcmp(cmd, "playlist_item_delete") == 0) {
				int item = (int)json_get_number_value_for_key(json, "item");
				if (item >= 0) gmu_core_playlist_item_delete(item);
			} else if (strcmp(cmd, "volume_set") == 0) {
				int vol = (int)json_get_number_value_for_key(json, "volume");
				int rel = (int)json_get_number_value_for_key(json, "relative");
				if (rel != 0) {
					int cv = gmu_core_get_volume();
					cv += rel;
					if (cv < 0) cv = 0;
					gmu_core_set_volume(cv);
				} else if (vol >= 0) {
					gmu_core_set_volume(vol);
				}
			} else if (strcmp(cmd, "ping") == 0) {
				gmu_http_ping(c);
			} else if (strcmp(cmd, "medialib_refresh") == 0) {
				gmu_core_medialib_start_refresh();
			} else if (strcmp(cmd, "medialib_search") == 0) {
				const char *type = json_get_string_value_for_key(json, "type");
				const char *str  = json_get_string_value_for_key(json, "str");
				if (type && str) {
					gmu_http_medialib_search(c, type, str);
				}
			} else if (strcmp(cmd, "medialib_add_id_to_playlist") == 0) {
				int id = json_get_number_value_for_key(json, "id");
				if (id > 0) gmu_core_medialib_add_id_to_playlist(id);
			} else if (strcmp(cmd, "medialib_browse") == 0) {
				const char *col = json_get_string_value_for_key(json, "column");
				if (col && strcmp(col, "artist") == 0) {
					gmu_http_medialib_browse_artists(c);
				}
			} else if (strcmp(cmd, "medialib_path_add") == 0) {
				const char *path = json_get_string_value_for_key(json, "path");
				if (path) gmu_core_medialib_path_add(path);
			}
		}
	}
	json_object_free(json);
	return res;
}

/**
 * Webserver main loop
 * listen_fd is the socket file descriptor where the server is 
 * listening for client connections.
 */
static void webserver_main_loop(int listen_fd)
{
	fd_set      the_state;
	int         maxfd;
	int         rb_ok = 1;
	Connection *con_ptr;
	Connection *first_connection = NULL;
	size_t      con_count = 0;

	FD_ZERO(&the_state);
	FD_SET(listen_fd, &the_state);
	maxfd = listen_fd;

	assign_signal_handler(SIGPIPE, SIG_IGN);

	while (server_running) {
		fd_set         readfds, exceptfds;
		int            ret;
		struct timeval tv;
		char          *websocket_msg;

		/* previously: 2 seconds timeout */
		tv.tv_sec  = 0;
		tv.tv_usec = 1000;

		readfds = the_state; /* select() changes readfds */
		exceptfds = the_state;
		ret = select(maxfd + 1, &readfds, NULL, &exceptfds, &tv);
		if (ret == -1 && errno == EINTR) {
			/* A signal has occured; ignore it. */
			continue;
		}
		if (ret < 0) {
			wdprintf(V_DEBUG, "httpd", "An error occured: (%d) %s\n", ret, strerror(errno));
		}

		/* Check TCP listen port for incoming connections... */
		if (FD_ISSET(listen_fd, &readfds)) {
			Connection ctmp = tcp_server_client_init(listen_fd);

			if (ctmp.state != CON_ERROR) {
				fcntl(ctmp.fd, F_SETFL, O_NONBLOCK);
				if (con_count < MAX_CONNECTIONS) {
					con_count++;
					if (ctmp.fd >= 0) {
						Connection *tmp_con;
						FD_SET(ctmp.fd, &the_state); /* add new client */
						if (ctmp.fd > maxfd) maxfd = ctmp.fd;
						wdprintf(V_DEBUG, "httpd", "Initializing new connection...\n");
						tmp_con = connection_init(ctmp.fd, ctmp.client_ip, first_connection);
						if (tmp_con && connection_get_state(tmp_con) != CON_ERROR) {
							wdprintf(V_DEBUG, "httpd", "Incoming connection %d. Connection count: %d (++)\n", ctmp.fd, con_count);
							first_connection = tmp_con;
						} else {
							wdprintf(V_DEBUG, "httpd", "Error with incoming connection %d.\n", ctmp.fd);
							connection_close(tmp_con);
						}
					}
				} else {
					wdprintf(
						V_WARNING,
						"httpd",
						"Connection limit reached of %d. Cannot accept incoming connection %d.\n",
						MAX_CONNECTIONS,
						con_count
					);
					close(ctmp.fd);
				}
			} else {
				wdprintf(V_WARNING, "httpd", "ERROR: Could not accept client connection.\n");
			}
		}

		/* Check WebSocket data send queue and fetch the next item if at
		 * least one is available */
		websocket_msg = queue_pop_alloc(&queue);

		/* Check all open TCP connections for incoming data. 
		 * Also handle ongoing http file requests and pending 
		 * WebSocket transmit requests here. */
		for (con_ptr = first_connection; con_ptr; ) {
			Connection *tmp_con = NULL;
			if (FD_ISSET(con_ptr->fd, &exceptfds)) {
				wdprintf(V_INFO, "httpd", "Error on connection %d.\n", con_ptr->fd);
				FD_CLR(con_ptr->fd, &the_state);
				if (con_ptr == first_connection) first_connection = con_ptr->next;
				tmp_con = con_ptr->next;
				connection_close(con_ptr);
				con_ptr = tmp_con;
				con_count--;
				continue;
			} else {
				if (connection_get_state(con_ptr) == CON_HTTP_BUSY) { /* feed data */
					/* Read CHUNK_SIZE bytes from file and send data to socket & update remaining bytes counter */
					connection_file_read_chunk(con_ptr);
				} else if (connection_get_state(con_ptr) == CON_WEBSOCKET_OPEN) {
					/* If data for sending through websocket has been fetched
					 * from the broadcast queue, send the data to all open WebSocket connections */
					if (websocket_msg) {
						if (connection_is_authenticated(con_ptr))
							websocket_send_string(con_ptr, websocket_msg);
					}
				}
				if (FD_ISSET(con_ptr->fd, &readfds)) { /* Data received on connection socket */
					char    msgbuf[MAXLEN+1];
					ssize_t msgbuflen;
					int     request_header_complete = 0;

					/* Read message from client */
					msgbuf[MAXLEN] = '\0';
					msgbuflen = sizeof(msgbuf);
					ret = tcp_server_read(con_ptr->fd, msgbuf, &msgbuflen);
					if (ret == ERROR) {
						FD_CLR(con_ptr->fd, &the_state); /* remove dead client */
						con_count--;
						wdprintf(V_INFO, "httpd", "Connection count: %d (--)\n", con_count);
						if (con_ptr == first_connection) first_connection = con_ptr->next;
						tmp_con = con_ptr->next;
						connection_close(con_ptr);
						con_ptr = tmp_con;
						continue;
					} else {
						size_t len = msgbuflen;
						size_t len_header = 0;

						if (connection_get_state(con_ptr) != CON_WEBSOCKET_OPEN) {
							char *tmp;
							wdprintf(V_DEBUG, "httpd", "%04d http message.\n", con_ptr->fd);
							if (con_ptr->http_request_header)
								len_header = strlen(con_ptr->http_request_header);
							tmp = realloc(con_ptr->http_request_header, len_header+len+1);
							if (tmp) {
								con_ptr->http_request_header = tmp;
							} else {
								connection_free_request_header(con_ptr);
							}
							if (con_ptr->http_request_header) {
								memcpy(con_ptr->http_request_header+len_header, msgbuf, len);
								con_ptr->http_request_header[len_header+len] = '\0';
								if (strstr(con_ptr->http_request_header, "\r\n\r\n") || 
								    strstr(con_ptr->http_request_header, "\n\n")) { /* we got a complete header */
									request_header_complete = 1;
								}
							}
						} else {
							char   tmp_buf[16];
							size_t size = 0;
							int    loop = 1;

							wdprintf(V_DEBUG, "httpd", "%04d websocket message received.\n", con_ptr->fd);
							if (msgbuflen > 0) {
								rb_ok = ringbuffer_write(&(con_ptr->rb_receive), msgbuf, msgbuflen);
								if (!rb_ok) wdprintf(V_WARNING, "httpd", "WARNING: Cannot write to ring buffer. Ring buffer full.\n");
							}

							do {
								ringbuffer_set_unread_pos(&(con_ptr->rb_receive));
								if (ringbuffer_read(&(con_ptr->rb_receive), tmp_buf, 10)) {
									size = websocket_calculate_packet_size(tmp_buf);
									wdprintf(V_DEBUG, "httpd", "Size of websocket message: %d bytes\n", size);
									ringbuffer_unread(&(con_ptr->rb_receive));
								}
								if (ringbuffer_get_fill(&(con_ptr->rb_receive)) >= size && size > 0) {
									char *wspacket = malloc(size+1); /* packet size+1 byte null terminator */
									char *payload;
									if (wspacket) {
										ringbuffer_read(&(con_ptr->rb_receive), wspacket, size);
										wspacket[size] = '\0';
										payload = websocket_unmask_message_alloc(wspacket, size);
										if (payload) {
											wdprintf(V_DEBUG, "httpd", "Payload data=[%s]\n", payload);
											if (!gmu_http_handle_websocket_message(payload, con_ptr)) {
												Connection *tmp_con = con_ptr;
												FD_CLR(con_ptr->fd, &the_state);
												if (con_ptr == first_connection) first_connection = con_ptr->next;
												con_count--;
												con_ptr = con_ptr->next;
												connection_close(tmp_con);
												free(payload);
												continue;
											}
											free(payload);
										}
										free(wspacket);
										size = 0;
									}
								} else if (size > 0) {
									wdprintf(
										V_DEBUG,
										"httpd", "Not enough data available. Need %d bytes, but only %d avail.\n",
										size,
										ringbuffer_get_fill(&(con_ptr->rb_receive))
									);
									ringbuffer_unread(&(con_ptr->rb_receive));
									loop = 0;
								} else {
									loop = 0;
								}
							} while (loop);
							connection_reset_timeout(con_ptr);
						}
					}
					if (request_header_complete)
						process_command(con_ptr->fd, con_ptr);
				} else { /* no data received */
					if (connection_is_valid(con_ptr) &&
						connection_is_timed_out(con_ptr) &&
						connection_get_state(con_ptr) != CON_WEBSOCKET_OPEN) { /* close idle connection */
						wdprintf(V_DEBUG, "httpd", "Closing connection to idle client %d...\n", con_ptr->fd);
						FD_CLR(con_ptr->fd, &the_state);
						if (con_ptr == first_connection) first_connection = con_ptr->next;
						tmp_con = con_ptr->next;
						con_count--;
						connection_close(con_ptr);
						con_ptr = tmp_con;
						wdprintf(V_INFO, "httpd", "Connection count: %d (--) (idle)\n", con_count);
						continue;
					}
				}
			}
			tmp_con = con_ptr;
			con_ptr = con_ptr->next;
			if (tmp_con->state == CON_ERROR) {
				if (first_connection == tmp_con) first_connection = con_ptr;
				connection_close(tmp_con);
			}
		}
		if (websocket_msg) free(websocket_msg);
	}
	queue_free(&queue);
	for (con_ptr = first_connection; con_ptr; ) {
		Connection *tmp_con = con_ptr->next;
		connection_close(con_ptr);
		con_ptr = tmp_con;
	}
	shutdown(listen_fd, SHUT_RDWR);
}

void httpd_stop_server(void)
{
	server_running = 0;
}

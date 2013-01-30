/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
#include <signal.h>
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
#include "wejpconfig.h"
#include "charset.h"

#define PASSWORD_MIN_LENGTH 9
#define INDEX_FILE "gmu.html"

/*
 * 500 Internal Server Error
 * 501 Not Implemented
 * 400 Bad Request
 * 505 HTTP Version not supported
 */

static Queue queue;
static char  webserver_root[256];

static char *http_status[] = {
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

static char *mime_type[] = {
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

static char *get_mime_type(char *url)
{
	char *ext, *res = "text/html";
	ext = strrchr(url, '.');
	if (ext) {
		int i;
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
static char *get_next_key_value_pair(char *str, char *key, int key_len, char *value, int value_len)
{
	int sc = 0, i = 0, eoh = 0;

	/* Check for end of header: "\n\r\n\r" */
	if ((str[sc] == '\r' && str[sc+1] == '\n') || str[sc] == '\n') {
		eoh = 1;
		wdprintf(V_DEBUG, "httpd", "End of header detected!\n");
	}

	key[0] = '\0';
	value[0] = '\0';
	if (!eoh) {
		/* extract key */
		char ch = 0;
		ch = str[0];
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

static Connection connection[MAX_CONNECTIONS];

static int server_running = 0;

typedef void (*sighandler_t)(int);

static sighandler_t my_signal(int sig_nr, sighandler_t signalhandler)
{
	struct sigaction neu_sig, alt_sig;

	neu_sig.sa_handler = signalhandler;
	sigemptyset(&neu_sig.sa_mask);
	neu_sig.sa_flags = SA_RESTART;
	if (sigaction(sig_nr, &neu_sig, &alt_sig) < 0)
		return SIG_ERR;
	return alt_sig.sa_handler;
}

static int websocket_send_string(Connection *c, char *str)
{
	int res = 0;
	if (str && c->fd) {
		res = websocket_send_str(c->fd, str, 0);
		if (!res)
			connection_close(c);
		else
			connection_reset_timeout(c);
	}
	return res;
}

int connection_init(Connection *c, int fd)
{
	ConfigFile *cf = gmu_core_get_config();
	memset(c, 0, sizeof(Connection));
	c->connection_time = time(NULL);
	c->state = HTTP_NEW;
	c->fd = fd;
	c->http_request_header = NULL;
	c->password_ref = NULL;
	if (cf) c->password_ref = cfg_get_key_value(*cf, "gmuhttp.Password");
	c->authentication_okay = 0;
	ringbuffer_init(&(c->rb_receive), 131072);
	return 1;
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
	// do stuff...
	if (c->http_request_header) {
		free(c->http_request_header);
		c->http_request_header = NULL;
	}
	ringbuffer_free(&(c->rb_receive));
	memset(c, 0, sizeof(Connection));
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
	int timeout = c->state == WEBSOCKET_OPEN ? 
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

int connection_file_open(Connection *c, char *filename)
{
	struct stat fst;
	if (c->local_file) fclose(c->local_file);
	c->total_size = 0;
	c->local_file = NULL;
	if (stat(filename, &fst) == 0) {
		if (S_ISREG(fst.st_mode)) {
			wdprintf(V_DEBUG, "httpd", "Connection: Opening file %s (%d bytes)...\n", filename, (int)fst.st_size);
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
			int size = CHUNK_SIZE;
			if (c->remaining_bytes_to_send < CHUNK_SIZE) size = c->remaining_bytes_to_send;
			wdprintf(V_DEBUG, "httpd", "Connection %d: Reading chunk of data: %d bytes\n", c->fd, size);
			/* read CHUNK_SIZE bytes from file */
			fread(blob, size, 1, c->local_file);
			/* write bytes to socket */
			net_send_block(c->fd, (unsigned char *)blob, size);
			/* decrement remaining bytes counter */
			c->remaining_bytes_to_send -= size;
			c->connection_time = time(NULL);
		} else {
			connection_set_state(c, HTTP_IDLE);
			if (c->local_file) fclose(c->local_file);
			c->local_file = NULL;
		}
		/* If eof, set connection state to HTTP_IDLE and close file, 
		 * set local_file to NULL */
		if (c->local_file && feof(c->local_file)) {
			fclose(c->local_file);
			c->local_file = NULL;
			connection_set_state(c, HTTP_IDLE);
		}
	} else {
		wdprintf(V_DEBUG, "httpd", "Connection: ERROR, file handle invalid.\n");
	}
	return 0;
}

int connection_authenticate(Connection *c, char *password)
{
	if (password && c->password_ref &&
	    strlen(password) >= PASSWORD_MIN_LENGTH &&
	    strcmp(password, c->password_ref) == 0) {
		c->authentication_okay = 1;
		websocket_send_string(c, "{\"cmd\":\"login\",\"res\":\"success\"}");
	} else {
		c->authentication_okay = 0;
		websocket_send_string(c, "{\"cmd\":\"login\",\"res\":\"failure\"}");
	}
	return c->authentication_okay;
}

static void send_http_header(int soc, char *code,
                             int length, time_t *time_modified,
                             const char *content_type)
{
	char       msg[255] = {0};
	struct tm *ptm = NULL;
	time_t     stime;
	char      *code_text = "";
	int        i;

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
	snprintf(msg, 254, "Content-Length: %d\r\n", length);
	net_send_buf(soc, msg);
	/*send_buf(soc, "Connection: close\r\n");*/
	snprintf(msg, 254, "Content-Type: %s\r\n", content_type);
	net_send_buf(soc, msg);
	net_send_buf(soc, "\r\n");
}

static void webserver_main_loop(int listen_fd);
static int  tcp_server_init(int port, int local_only);

void *httpd_run_server(void *data)
{
	int                port = SERVER_PORT, local_only = 1;
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
	webserver_main_loop(tcp_server_init(port, local_only));
	return NULL;
}

#define MAXLEN 4096

#define OKAY 0
#define ERROR (-1)

/*
 * Open server listen port 'port'
 * Returns socket fd
 */
static int tcp_server_init(int port, int local_only)
{
	int                listen_fd, ret, yes = 1;

	listen_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (listen_fd >= 0) {
		/* Avoid "Address already in use" error: */
		ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (ret >= 0) {
			char   port_str[6];
			struct addrinfo hints, *res;

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;  /* use IPv4 or IPv6, whichever */
			hints.ai_socktype = SOCK_STREAM;
			if (!local_only) hints.ai_flags = AI_PASSIVE;  /* fill in my IP for me */
			snprintf(port_str, 6, "%d", port);
			getaddrinfo(local_only ? "127.0.0.1" : NULL, port_str, &hints, &res);

			listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			ret = bind(listen_fd, res->ai_addr, res->ai_addrlen);

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

/*
 * Open connection to client. To be called for every new client.
 * Returns socket fs when successful, ERROR otherwise
 */
static int tcp_server_client_init(int listen_fd)
{
	int                fd;
	struct sockaddr_in sock;
	socklen_t          socklen;

	socklen = sizeof(sock);
	fd = accept(listen_fd, (struct sockaddr *) &sock, &socklen);

	return fd < 0 ? ERROR : fd;
}

/*
 * Write to client socket
 * Returns OKAY when the message could be written completely, ERROR otherwise
 */
static int tcp_server_write(int fd, char buf[], int buflen)
{
	return write(fd, buf, buflen) == buflen ? OKAY : ERROR;
}

/*
 * Read from client socket
 * Returns OKAY when reading was successful, ERROR otherwise
 */
static int tcp_server_read(int fd, char buf[], int *buflen)
{
	int res = OKAY;
	*buflen = read(fd, buf, *buflen);
	if (*buflen <= 0) { /* End of TCP connection */
		close(fd);
		res = ERROR; /* fd no longer valid */
	}
	return res;
}

static int is_valid_resource(char *str)
{
	int res = 1, i, len = 0;
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

static HTTPCommand get_command(char *str)
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
	char *str = "<h1>400 Bad Request</h1>";
	int   body_len = strlen(str);
	send_http_header(fd, "400", body_len, NULL, "text/html");
	if (!head_only) tcp_server_write(fd, str, body_len);
}

static void http_response_not_found(int fd, int head_only)
{
	char *str = "<h1>404 File not found</h1>";
	int   body_len = strlen(str);
	send_http_header(fd, "404", body_len, NULL, "text/html");
	if (!head_only) tcp_server_write(fd, str, body_len);
}

static void http_response_not_implemented(int fd)
{
		char *str =
				"<html><head><title>501 Not implemented</title></head>\n"
				"<body>\n<h1>501 Not implemented</h1>\n<p>Invalid method.</p>\n"
				"<p><hr /><i>Gmu http server</i></p>\n</body>\n"
				"</html>\r\n\r\n";
		int   body_len = strlen(str);
		send_http_header(fd, "501", body_len, NULL, "text/html");
		tcp_server_write(fd, str, body_len);
}

static int process_command(int rfd, Connection *c)
{
	char *command = NULL, *resource = NULL, *http_version = NULL, *options = NULL;
	char *host = NULL, websocket_key[32] = "";

	if (c->http_request_header) {
		int   len = strlen(c->http_request_header);
		int   i, state = 1;
		char *str;
		
		str = malloc(len+1);
		memset(str, 0, len); // Huh?
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
			char key[128], value[256], *opts = options;
			int  websocket_version = 0, websocket_upgrade = 0, websocket_connection = 0;
			wdprintf(V_DEBUG, "httpd", "%04d Options: [%s]\n", rfd, options);
			while (opts) {
				opts = get_next_key_value_pair(opts, key, 128, value, 256);
				if (key[0]) {
					wdprintf(V_DEBUG, "httpd", "key=[%s] value=[%s]\n", key, value);
					if (strcasecmp(key, "Host") == 0 && value[0]) {
						int len = strlen(value);
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
							char *websocket_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
							char str[61], str2[61];
							SHA1_CTX sha;
							uint8_t  digest[SHA1_DIGEST_SIZE];
							wdprintf(V_DEBUG, "httpd", "%04d Proceeding with websocket connection upgrade...\n", rfd);
							/* 1) Calculate response key (SHA1, Base64) */
							snprintf(str, 61, "%s%s", websocket_key, websocket_magic);
							str[60] = '\0';
							wdprintf(V_DEBUG, "httpd", "Voodoo = '%s'\n", str);
							SHA1_Init(&sha);
							SHA1_Update(&sha, (const uint8_t *)str, strlen(str));
							SHA1_Final(&sha, digest);
							memset(str, 0, 61);
							base64_encode_data((unsigned char *)digest, 20, str, 30);
							wdprintf(V_DEBUG, "httpd", "Calculated base 64 response value: '%s'\n", str);
							/* 2) Send 101 response with appropriate data */
							net_send_buf(rfd, "HTTP/1.1 101 Switching Protocols\r\n");
							net_send_buf(rfd, "Server: Gmu http server\r\n");
							net_send_buf(rfd, "Upgrade: websocket\r\n");
							net_send_buf(rfd, "Connection: Upgrade\r\n");
							snprintf(str2, 60, "Sec-WebSocket-Accept: %s\r\n", str);
							wdprintf(V_DEBUG, "httpd", str2);
							net_send_buf(rfd, str2);
							net_send_buf(rfd, "\r\n");
							/* 3) Set flags in connection struct to WebSocket */
							connection_set_state(c, WEBSOCKET_OPEN);
							websocket_send_string(c, "{ \"cmd\": \"hello\" }");
						} else if (!connection_file_is_open(c)) { // ?? open file (if not open already) ??
							char filename[512];
							memset(filename, 0, 512);
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
								if (!head_only) connection_set_state(c, HTTP_BUSY);
								send_http_header(rfd, "200",
												 connection_get_number_of_bytes_to_send(c),
												 time_ok ? &(st.st_ctime) : NULL,
												 get_mime_type(resource));
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
void httpd_send_websocket_broadcast(char *str)
{
	queue_push(&queue, str);
}

#define MSG_MAX_LEN 1024

void gmu_http_playlist_get_info(Connection *c)
{
	char msg[MSG_MAX_LEN];
	int  r = snprintf(msg, MSG_MAX_LEN,
	                  "{ \"cmd\": \"playlist_info\", \"changed_at_position\" : 0, \"length\" : %d }",
	                  gmu_core_playlist_get_length());
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
}

void gmu_http_playlist_get_item(int id, Connection *c)
{
	char   msg[MSG_MAX_LEN], *tmp_title = NULL;
	Entry *item = gmu_core_playlist_get_entry(id);
	int    r;

	tmp_title = json_string_escape_alloc(gmu_core_playlist_get_entry_name(item));
	r = snprintf(msg, MSG_MAX_LEN,
	             "{ \"cmd\": \"playlist_item\", \"position\" : %d, \"title\": \"%s\", \"length\": %d }",
	             id, tmp_title ? tmp_title : "??", 0);
	if (tmp_title) free(tmp_title);
	if (r > 0 && !charset_is_valid_utf8_string(msg)) {
		r = snprintf(msg, MSG_MAX_LEN,
		             "{ \"cmd\": \"playlist_item\", \"position\" : %d, \"title\": \"(Invalid UTF-8)\", \"length\": %d }",
		             id, 0);
	}
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
}

void gmu_http_get_current_trackinfo(Connection *c)
{
	TrackInfo *ti = gmu_core_get_current_trackinfo_ref();
	char msg[MSG_MAX_LEN];
	int  r = snprintf(msg, MSG_MAX_LEN,
	                  "{ \"cmd\": \"trackinfo\", \"artist\": \"%s\", \"title\": \"%s\", \"album\": \"%s\", \"date\": \"%s\", \"length_min\": %d, \"length_sec\": %d, \"pl_pos\": %d  }",
	                  trackinfo_get_artist(ti),
	                  trackinfo_get_title(ti),
	                  trackinfo_get_album(ti),
	                  trackinfo_get_date(ti),
	                  trackinfo_get_length_minutes(ti),
	                  trackinfo_get_length_seconds(ti),
	                  0);
	if (r > 0 && !charset_is_valid_utf8_string(msg)) {
		snprintf(msg, MSG_MAX_LEN,
		         "{ \"cmd\": \"trackinfo\", \"artist\": \"(Invalid UTF-8)\", \"title\": \"(Invalid UTF-8)\", \"album\": \"(Invalid UTF-8)\", \"date\": \"(Invalid UTF-8)\", \"length_min\": %d, \"length_sec\": %d, \"pl_pos\": %d  }",
		         trackinfo_get_length_minutes(ti),
	             trackinfo_get_length_seconds(ti),
	             0);
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
	int  r = snprintf(msg, MSG_MAX_LEN,
	                  "{ \"cmd\": \"playlist_change\", \"changed_at_position\" : 0, \"length\" : %d }",
	                  gmu_core_playlist_get_length());
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
	r = snprintf(msg, MSG_MAX_LEN,
	             "{ \"cmd\": \"playback_state\", \"state\" : %d }",
	             gmu_core_get_status());
	if (r < MSG_MAX_LEN && r > 0) websocket_send_string(c, msg);
}

#define MAX_LEN 32768

static void gmu_http_read_dir(char *directory, Connection *c)
{
	Dir   dir;
	char *base_dir;

	dir_init(&dir);
	ConfigFile *cf = gmu_core_get_config();
	base_dir = cfg_get_key_value(*cf, "gmuhttp.BaseDir");
	if (!base_dir) base_dir = "/";
	if (strncmp(base_dir, directory, strlen(base_dir)) != 0)
		directory = base_dir;
	dir_set_base_dir(&dir, base_dir);
	dir_set_ext_filter(gmu_core_get_file_extensions(), 1);

	if (dir_read(&dir, directory, 1)) {
		int i = 0, num_files = dir_get_number_of_files(&dir);
		while (i < num_files) {
			char res[MAX_LEN], *jpath = NULL, *spath;
			int  pos;

			spath = dir_get_new_dir_alloc("/", directory);
			if (spath) {
				jpath = json_string_escape_alloc(spath);
				free(spath);
			}
			if (jpath) {
				snprintf(res, MAX_LEN, "{ \"cmd\": \"dir_read\", \"res\": \"ok\", \"path\": \"%s\", \"data\": {", jpath);
				free(jpath);
				for (pos = strlen(res); i < num_files; i++) {
					char *tmp = json_string_escape_alloc(dir_get_filename(&dir, i));
					int   filesize = dir_get_filesize(&dir, i);
					int   pos_prev = pos;
					if (tmp) {
						snprintf(res+pos, MAX_LEN-pos, "\"%d\": { \"name\": \"%s\", \"size\": %d, \"is_dir\": %d },",
								 i, tmp, filesize, dir_get_flag(&dir, i) == DIRECTORY);
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
	dir_free(&dir);
}

static void gmu_http_handle_websocket_message(char *message, Connection *c)
{
	JSON_Object *json = json_parse_alloc(message);
	if (json && !json_object_has_parse_error(json)) { /* Valid JSON data received */
		/* Analyze command in JSON data */
		char *cmd = json_get_string_value_for_key(json, "cmd");
		if (cmd && strcmp(cmd, "login") == 0) {
			char *password = json_get_string_value_for_key(json, "password");
			if (!connection_authenticate(c, password))
				connection_close(c);
			else
				gmu_http_send_initial_information(c);
		} else if (cmd && connection_is_authenticated(c)) {
			wdprintf(V_DEBUG, "httpd", "Got command (via JSON data): '%s'\n", cmd);
			if (strcmp(cmd, "play") == 0) {
				JSON_Key_Type type = json_get_type_for_key(json, "item");
				int           item = (int)json_get_number_value_for_key(json, "item");
				if (type == NUMBER && item >= 0) {
					gmu_core_play_pl_item(item);
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
				char *dir = json_get_string_value_for_key(json, "dir");
				if (dir) {
					gmu_http_read_dir(dir, c);
				}
			} else if (strcmp(cmd, "get_current_track") == 0) {
			} else if (strcmp(cmd, "playlist_add") == 0) {
				char *path = json_get_string_value_for_key(json, "path");
				char *type = json_get_string_value_for_key(json, "type");
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
			} else if (strcmp(cmd, "playlist_playmode_get_info") == 0) {
				gmu_http_playmode_get_info(c);
			} else if (strcmp(cmd, "playlist_item_delete") == 0) {
				int item = (int)json_get_number_value_for_key(json, "item");
				if (item >= 0) gmu_core_playlist_item_delete(item);
			}
		}
	}
	json_object_free(json);
}

/* 
 * Webserver main loop
 * listen_fd is the socket file descriptor where the server is 
 * listening for client connections.
 */
static void webserver_main_loop(int listen_fd)
{
	fd_set the_state;
	int    maxfd;
	int    rb_ok = 1, i;

	FD_ZERO(&the_state);
	FD_SET(listen_fd, &the_state);
	maxfd = listen_fd;

	my_signal(SIGPIPE, SIG_IGN);

	server_running = 1;
	while (server_running) {
		fd_set         readfds;
		int            ret, rfd;
		struct timeval tv;
		char          *websocket_msg;

		/* previously: 2 seconds timeout */
		tv.tv_sec  = 0;
		tv.tv_usec = 1000;

		readfds = the_state; /* select() changes readfds */
		ret = select(maxfd + 1, &readfds, NULL, NULL, &tv);
		if ((ret == -1) && (errno == EINTR)) {
			/* A signal has occured; ignore it. */
			continue;
		}
		if (ret < 0) break;

		/* Check TCP listen port for incoming connections... */
		if (FD_ISSET(listen_fd, &readfds)) {
			int con_num;
			rfd = tcp_server_client_init(listen_fd);
			if (rfd != ERROR) {
				fcntl(rfd, F_SETFL, O_NONBLOCK);
				con_num = rfd-listen_fd-1;
				if (con_num < MAX_CONNECTIONS) {
					if (rfd >= 0) {
						FD_SET(rfd, &the_state); /* add new client */
						if (rfd > maxfd) maxfd = rfd;
						connection_init(&(connection[con_num]), rfd);
						wdprintf(V_DEBUG, "httpd", "Incoming connection %d. Connection count: ++\n", con_num);
					}
				} else {
					wdprintf(V_WARNING, "httpd",
							 "Connection limit reached of %d. Cannot accept incoming connection %d.\n",
							 MAX_CONNECTIONS, con_num);
					close(rfd);
				}
			} else {
				wdprintf(V_WARNING, "httpd", "ERROR: Could not accept client connection.");
			}
		}

		/* Check WebSocket data send queue and fetch the next item if at
		 * least one is available */
		websocket_msg = queue_pop_alloc(&queue);

		/* Check all open TCP connections for incoming data. 
		 * Also handle ongoing http file requests and pending 
		 * WebSocket transmit requests here. */
		for (rfd = listen_fd + 1; rfd <= maxfd; ++rfd) {
			int conn_num = rfd-listen_fd-1;
			if (connection_get_state(&(connection[conn_num])) == HTTP_BUSY) { /* feed data */
				/* Read CHUNK_SIZE bytes from file and send data to socket & update remaining bytes counter */
				connection_file_read_chunk(&(connection[conn_num]));
			} else if (connection[conn_num].state == WEBSOCKET_OPEN) {
				/* If data for sending through websocket has been fetched
				 * from the broadcast queue, send the data to all open WebSocket connections */
				if (websocket_msg) {
					if (connection_is_authenticated(&(connection[conn_num])))
						websocket_send_string(&(connection[conn_num]), websocket_msg);
				}
			}
			if (FD_ISSET(rfd, &readfds)) { /* Data received on connection socket */
				char msgbuf[MAXLEN+1];
				int  msgbuflen, request_header_complete = 0;

				/* Read message from client */
				msgbuf[MAXLEN] = '\0';
				msgbuflen = sizeof(msgbuf);
				ret = tcp_server_read(rfd, msgbuf, &msgbuflen);
				if (ret == ERROR) {
					FD_CLR(rfd, &the_state); /* remove dead client */
					wdprintf(V_DEBUG, "httpd", "Connection count: --\n");
				} else {
					int len = msgbuflen;
					int len_header = 0;

					if (connection[conn_num].state != WEBSOCKET_OPEN) {
						wdprintf(V_DEBUG, "httpd", "%04d http message.\n", rfd);
						if (connection[conn_num].http_request_header)
							len_header = strlen(connection[conn_num].http_request_header);
						connection[conn_num].http_request_header = 
							realloc(connection[conn_num].http_request_header, len_header+len+1);
						if (connection[conn_num].http_request_header) {
							memcpy(connection[conn_num].http_request_header+len_header, msgbuf, len);
							connection[conn_num].http_request_header[len_header+len] = '\0';
						}
						if (strstr(connection[conn_num].http_request_header, "\r\n\r\n") || 
							strstr(connection[conn_num].http_request_header, "\n\n")) { /* we got a complete header */
							request_header_complete = 1;
						}
					} else {
						char tmp_buf[16];
						int  size = 0, loop = 1;

						wdprintf(V_DEBUG, "httpd", "%04d websocket message received.\n", rfd);
						if (msgbuflen > 0) {
							rb_ok = ringbuffer_write(&(connection[conn_num].rb_receive), msgbuf, msgbuflen);
							if (!rb_ok) wdprintf(V_WARNING, "httpd", "WARNING: Cannot write to ring buffer. Ring buffer full.\n", rfd);
						}
						
						do {
							ringbuffer_set_unread_pos(&(connection[conn_num].rb_receive));
							if (ringbuffer_read(&(connection[conn_num].rb_receive), tmp_buf, 10)) {
								size = websocket_calculate_packet_size(tmp_buf);
								wdprintf(V_DEBUG, "httpd", "Size of websocket message: %d bytes\n", size);
								ringbuffer_unread(&(connection[conn_num].rb_receive));
							}
							if (ringbuffer_get_fill(&(connection[conn_num].rb_receive)) >= size && size > 0) {
								char *wspacket = malloc(size+1); /* packet size+1 byte null terminator */
								char *payload;
								if (wspacket) {
									ringbuffer_read(&(connection[conn_num].rb_receive), wspacket, size);
									wspacket[size] = '\0';
									payload = websocket_unmask_message_alloc(wspacket, size);
									if (payload) {
										wdprintf(V_DEBUG, "httpd", "Payload data=[%s]\n", payload);
										gmu_http_handle_websocket_message(payload, &(connection[conn_num]));
										free(payload);
									}
									free(wspacket);
									size = 0;
								}
							} else if (size > 0) {
								wdprintf(V_DEBUG,
										 "httpd", "Not enough data available. Need %d bytes, but only %d avail.\n",
										 size, ringbuffer_get_fill(&(connection[conn_num].rb_receive)));
								ringbuffer_unread(&(connection[conn_num].rb_receive));
								loop = 0;
							} else {
								loop = 0;
							}
						} while (loop);
						connection_reset_timeout(&(connection[conn_num]));
					}
				}
				if (request_header_complete)
					process_command(rfd, &(connection[conn_num]));
			} else { /* no data received */
				if (connection_is_valid(&(connection[conn_num])) &&
					connection_is_timed_out(&(connection[conn_num])) &&
					connection[conn_num].state != WEBSOCKET_OPEN) { /* close idle connection */
					wdprintf(V_DEBUG, "httpd", "Closing connection to idle client %d...\n", rfd);
					FD_CLR(rfd, &the_state);
					close(rfd);
					connection_close(&(connection[conn_num]));
					wdprintf(V_DEBUG, "httpd", "Connection count: -- (idle)\n");
				}
			}
		}
		if (websocket_msg) free(websocket_msg);
	}
	queue_free(&queue);
	for (i = 0; i < MAX_CONNECTIONS; i++)
		connection_close(&(connection[i]));
}

void httpd_stop_server(void)
{
	server_running = 0;
}

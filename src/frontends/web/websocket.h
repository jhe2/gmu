/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File:websocket.h  Created: 120930
 *
 * Description: Websocket functions
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H
char *websocket_unmask_message_alloc(const char *msgbuf, int msgbuf_size);
char *websocket_prepare_message_from_str_alloc(const char *str, int mask);
char *websocket_client_generate_sec_websocket_key_alloc(void);
int   websocket_send_str(int sock, const char *str, int mask);
int   websocket_calculate_payload_size(const char *websocket_packet_header);
const char *websocket_get_payload(const char *websocket_packet);
int   websocket_calculate_packet_size(const char *websocket_packet);
#endif

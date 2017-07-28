//
// Created by dxing97 on 7/27/17.
//

/*
 * websockets.h:
 *
 * functions and stuff used to service the websocket connection
 */

#ifndef LIBDISCORD_WEBSOCKETS_H
#define LIBDISCORD_WEBSOCKETS_H

#include "libdiscord.h"
#include <libwebsockets.h>



/*
 * the lws user-defined callback for websocket events like client_recieve, client_writable, peer_initiated_close, etc
 *
 * todo: clean it up
 */
int callback_discord(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

/*
 * lws protocols sent to the gateway when connecting
 */
extern struct lws_protocols protocols[2];

/*
 * extensions supported by our websocket, currently not relevant to libdiscord (no RFC 1950 implementation yet)
 */
extern struct lws_extension exts[2];

/*
 * creates a lws context based on Discord API parameters
 */
struct lws_context *ld_create_lws_context();

#endif //LIBDISCORD_WEBSOCKETS_H
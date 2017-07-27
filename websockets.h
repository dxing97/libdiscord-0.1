//
// Created by dxing97 on 7/27/17.
//

#ifndef LIBDISCORD_WEBSOCKETS_H
#define LIBDISCORD_WEBSOCKETS_H

//#include "libdiscord.h"
#include <libwebsockets.h>

int
callback_discord(struct lws *wsi, enum lws_callback_reasons reason, void *user,
                 void *in, size_t len);

extern struct lws_protocols protocols[2];

extern static const struct lws_extension exts[2];

struct lws_context *ld_create_lws_context();

#endif //LIBDISCORD_WEBSOCKETS_H
//
// Created by dxing97 on 7/27/17.
//

#ifndef LIBDISCORD_WEBSOCKETS_H
#define LIBDISCORD_WEBSOCKETS_H

#include "libdiscord.h"

extern struct lws_protocols protocols[2];

extern static const struct lws_extension exts[2];

struct lws_context *ld_create_lws_context();

#endif //LIBDISCORD_WEBSOCKETS_H
//
// Created by dxing97 on 7/22/17.
//

#ifndef LIBDISCORD_DISCORD_H
#define LIBDISCORD_DISCORD_H

#endif //LIBDISCORD_DISCORD_H

#ifndef LIBJANSSON_INCLUDE
#include <jansson.h>

#endif

#ifndef LIBWEBSOCKETS_INCLUDE
#include <libwebsockets.h>

#endif

#define MAX_DISCORD_PAYLOAD 1024



enum ld_opcode {
    //discord dispatch opcodes as of API v6
    LD_OPCODE_NO_OP = -1,
    LD_OPCODE_DISPATCH = 0,
    LD_OPCODE_HEARTBEAT = 1,
    LD_OPCODE_IDENTIFY = 2,
    LD_OPCODE_STATUS_UPDATE = 3,
    LD_OPCODE_VOICE_STATE_UPDATE = 4,
    LD_OPCODE_VOICE_SERVER_PING = 5,
    LD_OPCODE_RESUME = 6,
    LD_OPCODE_RECONNECT = 7,
    LD_OPCODE_REQUEST_GUILD_MEMBERS = 8,
    LD_OPCODE_INVALID_SESSION = 9,
    LD_OPCODE_HELLO = 10,
    LD_OPCODE_HEARTBEAT_AK = 11
};

struct per_session_data__discord{
    size_t rx, tx;
    unsigned char buf[LWS_PRE + MAX_DISCORD_PAYLOAD];
    unsigned int len;
    unsigned int index;
    int final;
    int continuation;
    int binary;
    enum ld_opcode opcode;
    json_t *tx_data;
    const char *token;
};


enum ld_opcode ld_payloadbuf_get_opcode(char *payload, size_t len);

enum ld_opcode ld_payloadstr_get_opcode(char *payload);

json_t* ld_create_payload_identify(const char bot_token[]);

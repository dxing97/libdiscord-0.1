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

#include <zlib.h>

#include "libdiscordConfig.h"

#define MAX_DISCORD_PAYLOAD 4096

#define LD_BASE_URL "https://discordapp.com/api/v6"
#define LD_DISCORDAPI_VERSION "6"
#define LD_DISCORDAPI_WSENCODING "json"

enum ld_ws_state {
    LD_WSSTATE_NOT_CONNECTED = 0,
    LD_WSSTATE_CONNECTING = 1,
    LD_WSSTATE_CONNECTED_NOT_IDENTIFIED = 2,
    LD_WSSTATE_CONNECTED_IDENTIFIED = 3
};

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

int
callback_discord(struct lws *wsi, enum lws_callback_reasons reason, void *user,
                 void *in, size_t len);



/*
 * context required for opening and maintaining a websocket to discord
 *
 * gateway_url: the string containing the URL to connect to, get it from ld_get_gateway or ld_get_gateway_bot
 *      example: wss://gateway.discord.gg
 */

struct ld_sessiondata {
    char *bot_token;
    char *current_game;
    char *gateway_url;
    enum ld_ws_state ws_state;
    struct ld_wsdata *wsd;
    int last_seq_num;
    int heartbeat_interval;

};

struct ld_wsdata{ //websocket data-created and destroyed with the websocket
    size_t rx, tx;
    unsigned char buf[LWS_PRE + MAX_DISCORD_PAYLOAD];
    unsigned int len;
    unsigned int index;
    int final;
    int continuation;
    int binary;
    enum ld_opcode opcode;
};


enum ld_opcode ld_payloadbuf_get_opcode(char *payload, size_t len);
/*
 * takes a websocket payload char array of size len in JSON format and returns the opcode
 *
 * inputs:
 *  char *  payload
 *      pointer to the beginning of the char array
 *  size_t  len
 *      length of the char array that contains the JSON data
 *
 * will take into account arrays that are already null-terminated.
 */

enum ld_opcode ld_payloadstr_get_opcode(char *payload);
/*
 * takes a websocket payload null-terminated string in JSON format and returns the opcode
 *
 * inputs:
 *  char *  payload
 *      pointer to the beginning of the string
 *
 * extremely similar to ld_payloadbuf_get_opcode
 */

/*
 * create a lws context based on default discord settings
 */
struct lws_context * ld_create_lws_context();

/*
 * create a lws client connect info struct based on defaults and parameters
 */
struct lws_client_connect_info * ld_create_lws_connect_info(struct lws_context *context, char *gateway_url);


const char * ld_get_gateway();
/*
 * makes a GET request to the GET /gateway endpoint for the websocket URI.
 * returns string containing the websocket URL
 *
 * example: wss://gateway.discord.gg
 */

const char * ld_get_gateway_bot();
/*
 * makes a GET request to the GET /gateway/bot endpoint for the websocket URI
 */

json_t *ld_create_payload_from_context(struct ld_wsdata *sd);
/*
 * generate a JSON payload based on context data
 * calls specific create_payload functions based on opcode
 */

json_t * ld_create_payload_identify(struct ld_sessiondata *sd);
/*
 * creates a identify payload based on a bot token
 */

json_t * ld_create_payload_heartbeat(int * sequence_number);
/*
 * creates a heartbeat payload using a sequence number
 * note that the sequence number can be null.
 */

json_t * ld_create_payload_status_update ();

json_t * ld_create_payload_request_guild_members();

json_t * ld_create_payload_resume ();

json_t * ld_create_payload_request_members();

json_t * ld_create_payload_guild_sync();


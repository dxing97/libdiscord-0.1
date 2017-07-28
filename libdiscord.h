//
// Created by dxing97 on 7/22/17.
//

/*
 * libdiscord.h: functions that do things and stuff with Discord for reasons
 */

#ifndef LIBDISCORD_DISCORD_H
#define LIBDISCORD_DISCORD_H

#include <jansson.h>
#include <libwebsockets.h>

#include <sys/time.h>
#include <sys/queue.h>

#include "libdiscordConfig.h"

#include "html.h"
#include "json.h"
#include "websockets.h"

#define MAX_DISCORD_PAYLOAD 4096

#define LD_BASE_URL "https://discordapp.com/api/v6"
#define LD_DISCORDAPI_VERSION "6"
#define LD_DISCORDAPI_WSENCODING "json"

/*
 * describes the current state of our websocket connection to the gateway
 */
enum ld_ws_state {
    LD_WSSTATE_NOT_CONNECTED = 0,
    LD_WSSTATE_CONNECTING = 1,
    LD_WSSTATE_CONNECTED_NOT_IDENTIFIED = 2,
    LD_WSSTATE_CONNECTED_IDENTIFIED = 3,
    LD_WSSTATE_SENDING_PAYLOAD = 4
};

/*
 * different opcodes that can be sent and recieved in the websocket
 * note that some opcodes can only be sent, can only be recieved, or both
 */
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

/*
 * structs that go into our send queue, each containing a json of the payload we want to send
 */
struct ld_sq_entry{
    //JSON to send
    json_t *payload;

    TAILQ_ENTRY(entry) entries;
};

TAILQ_HEAD(ld_sendqueue, ld_sq_entry);

/*
 * context struct used for creating a ld_sessiondata struct using ld_create_sessiondata
 * bot token is required
 * initial game is not required
 */
struct ld_create_sessiondata_context {
    char *bot_token;
    char *game;
};
/*
 * struct containing all the information needed to connect to and maintain a connection to discord (REST and websocket)
 * persists between websocket connections
 *
    char *bot_token;
        string containing Discord bot token
    char *current_game;
        string containing the current "game" being played
    char *gateway_url;
        string containing the gateway URL
        use ld_get_gateway to get it
    enum ld_ws_state ws_state;
        enum detailing the current state of the websocket
    struct ld_wsdata *wsd;
        pointer to the lws allocated user data, created and destroyed with each websocket connection
    int last_seq_num;
        the last DISPATCH sequence number we've recieved.
        each recieved dispatch has a (probably) unique sequence number which increments up as we recieve dispatches
        -1 for no sequence number (i.e. we haven't recieved any dispatches yet)
    long heartbeat_interval;
        how long to wait between heartbeats, heartbeats can be sent at a slower rate than this
        in milliseconds
    long last_heartbeat;
        the time we recieved the last heartbeat, in milliseconds since the Epoch
    int first_heartbeat;
        0 if we haven't sent our first eartbeat yet
        1 if we have sent at least one heartbeat
    struct ld_sendqueue *sq;
        the TAILQ containing the send queue: stuff to be sent goes here
    struct lws *wsi;
        pointer to the lws wsi, null if we haven't initialized lws and connected to the gateway yet

 */

struct ld_sessiondata {
    char *bot_token;
    char *current_game;
    char *gateway_url;
    enum ld_ws_state ws_state;
    struct ld_wsdata *wsd;
    int last_seq_num;
    long heartbeat_interval;
    long last_heartbeat;
    int first_heartbeat;
    struct ld_sendqueue *sq;
    struct lws *wsi;
};

/*
 * global variable for session data
 */
extern struct ld_sessiondata sd;

/*
 * global variable for forcing an exit, should probably get rid of this
 */
extern int force_exit;

/*
 * library data created and destroyed with the websocket
 */
struct ld_wsdata{
    size_t rx, tx;
    unsigned char buf[LWS_PRE + MAX_DISCORD_PAYLOAD];
    unsigned int len;
    unsigned int index;
    int final;
    int continuation;
    int binary;
    enum ld_opcode opcode;
};

/*
 * called when SIGALRM from ualarm() is sent
 * creates a heartbeat payload and adds it to the send queue
 */
void ld_hbhandler(struct ld_sessiondata *sd);

/*
 * initialize the sessiondata struct based on the context information.
 *
 * context requires the bot token
 */
struct ld_sessiondata *ld_create_sessiondata(struct ld_create_sessiondata_context *context);


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
 * takes the HELLO payload and retrieves the heartbeat_interval value
 */
int ld_payload_hello_get_hb_interval(const char *payload);

/*
 * takes the DISPATCH payload and retrieves the sequence number value
 */
int ld_payload_dispatch_get_seqnum(const char *payload);

/*
 * create a lws client connect info struct based on Discord API parameters user parameters
 * (will use sessiondata in the future)
 */
struct lws_client_connect_info * ld_create_lws_connect_info(struct lws_context *context, char *gateway_url);

/*
 * makes a GET request to the GET /gateway endpoint for the websocket URI.
 * returns string containing the websocket URL
 *
 * example: wss://gateway.discord.gg
 */
const char * ld_get_gateway();

/*
 * makes a GET request to the GET/gateway/bot endpoint
 * returns a string containing the websocket URL
 *
 * future:
 * will return 1 on success and 0 on failure, stores gateway URI and shard number in sessiondata
 */
const char * ld_get_gateway_bot(struct ld_sessiondata *sd);

/*
 * generate a JSON payload based on context data
 * calls specific create_payload functions based on opcode
 *
 * NOT YET IMPLEMENTED
 */
json_t *ld_create_payload_from_context(struct ld_wsdata *sd, enum ld_opcode);

/*
 * creates a identify payload based on a bot token
 */
json_t * ld_create_payload_identify(struct ld_sessiondata *sd);

/*
 * creates a heartbeat payload using a sequence number
 * note that the sequence number can be null.
 */
json_t * ld_create_payload_heartbeat(int sequence_number);

/*
 * creates a STATUS_UPDATE payload
 *
 * NOT YET IMPLEMENTED
 */
json_t * ld_create_payload_status_update ();

/*
 * creates a REQUEST_GUILD_MEMBERS payload
 *
 * NOT YET IMPLEMENTED
 */
json_t * ld_create_payload_request_guild_members();

/*
 * creates a RESUME payload
 *
 * NOT YET IMPLEMENTED
 */
json_t * ld_create_payload_resume ();

/*
 * creates a REQUEST_MEMBERS payload
 *
 * NOT YET IMPLEMENTED
 */
json_t * ld_create_payload_request_members();

/*
 * creates a GUILD_SYNC payload
 *
 * NOT YET IMPLEMENTED
 */
json_t * ld_create_payload_guild_sync();

#endif //LIBDISCORD_DISCORD_H
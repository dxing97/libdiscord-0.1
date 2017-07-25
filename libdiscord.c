//
// Created by dxing97 on 7/22/17.
//

#include <string.h>
#include <jansson.h>
#include "libdiscord.h"
#include <ulfius.h>

struct ld_wsdata ld_currentsessiondata;

struct lws_protocols protocols[] = { //array of protocols that can be used
        /* first protocol must always be HTTP handler */

        {
                "Discordv6",		/* name of the protocol- can be overridden with -e */
                callback_discord, //the callback function for this protocol
                            sizeof(struct ld_wsdata),
                /* per_session_data_size - total memory allocated for a connection, allocated on connection establishment and freed on connection takedown */
                //note that this memory is given to the callback to work with
                MAX_DISCORD_PAYLOAD, //rx_buffer_size - space for rx data (duh)
                //id (unsigned int), - ignored by lws, can be used by user code for things like protocol version
                //user (void *), - ignored by lws, can be used by user code for things and stuff
                //tx_packet_size - 	space for tx data (duh)
                //	0 means set it to the same size as rx_buffer_size,
                //	otherwise tx data is sent in chunks of this size, which can be undesireable, so make sure the size is appropriate

        },
        {
                NULL, NULL, 0		/* End of list */
        }
};

static const struct lws_extension exts[] = {
        {
                "permessage-deflate", //built in extension - RFC7629 compression extension
                lws_extension_callback_pm_deflate,
                "permessage-deflate; client_no_context_takeover; client_max_window_bits"
        },
        { NULL, NULL, NULL /* terminator */ }
};


enum ld_opcode ld_payloadbuf_get_opcode(char *payload, size_t len) {
    json_t *decoded_payload, *opcode;
    json_error_t json_error;
    size_t json_flags = 0, nlen = 0; //no flags, see jansson documentation for possible decode flags
    enum ld_opcode returnop;
    int i;
    char *npayload;

    //see if there's a null byte in payload
    for (i = 0; i < (int) len + 3; i++) { //stop at second to last byte
//        printf("%d, ", i);
        /*
         * if the null byte comes before expected length (len), then it's probably fine
         * otherwise, create a new string with an added null terminator
         */
        if (payload[i] == '\0') {
//            printf("found null byte\n");
            nlen = (size_t) i;
            break; //it's a proper string
        }
    }
    if ((payload [i + 1] != '\0') && (payload[i] != '\0')) {
        //if the very last and second to last byte aren't null bytes, then we have to add our own
        npayload = (char *) malloc(len);
        if (npayload == NULL) {
            fprintf(stderr, "memory allocation error at ld_payloadbuf_get_opcode\n");
            return LD_OPCODE_NO_OP;
        }
        nlen = (size_t) i + 1;
        npayload = strncpy(npayload, payload, nlen);
        npayload[nlen - 1] = '\0';

    } else {
        npayload = payload;
    }

    decoded_payload = json_loadb(npayload, nlen, json_flags, &json_error);

    if (decoded_payload == NULL) {
        //didn't find an opcode or payload is mangled: is this really a payload?
        fprintf(stderr, "couldn't decode payload: couldn't read JSON format: %s\n"
                        "source: %s, line:%d, col:%d, pos:%d\n",
                json_error.text, json_error.source, json_error.line, json_error.column, json_error.position);
        return LD_OPCODE_NO_OP;
    }
    /*
     * for discord API v6: dispatches look like this
     *
{
    "op": 0,
    "d": {},
    "s": 42,
    "t": "GATEWAY_EVENT_NAME"
}
     * the opcode is what we want
     */

    opcode = json_object_get(decoded_payload, "op");
    if (opcode == NULL) {
        //didn't find an opcode or payload is mangled: is this really a payload?
        fprintf(stderr, "couldn't get opcode for payload: couldn't find key \"op\": %s\n"
                "source: %s, line:%d, col:%d, pos:%d\n",
                json_error.text, json_error.source, json_error.line, json_error.column, json_error.position);
        return LD_OPCODE_NO_OP;
    }

    if (json_is_integer(opcode) == 0) {
        //this key isn't an integer, where did this payload come from?
        //can check what type it is here
        fprintf(stderr, "couldn't get opcode for payload: key value is not an integer");
        return LD_OPCODE_NO_OP;
    }

    returnop = (enum ld_opcode) json_integer_value(opcode);
    if (returnop < 0 || returnop > LD_OPCODE_HEARTBEAT_AK) {
        //unknown opcode, wth?
        fprintf(stderr, "couldn't get opcode for payload: key value not valid opcode");
        return LD_OPCODE_NO_OP;
    }

    return returnop;

}

enum ld_opcode ld_payloadstr_get_opcode(char *payload) { //for null-terminated strings
    return ld_payloadbuf_get_opcode(payload, strlen(payload));
}

json_t* ld_create_payload_identify(struct ld_sessiondata *sd) {
    json_t *payload, *data, *properties, *presence, *game;
    char lv[128];
    sprintf(lv, "%s v%d.%d.%d", LD_NAME, LD_VERSION_MAJOR, LD_VERSION_MINOR, LD_VERSION_PATCH);

    payload = json_object();
    if (payload == NULL) {
        fprintf(stderr, "couldn't create payload JSON object for IDENTIFY payload\n");
        return NULL;
    }

    data = json_object();
    if (data == NULL) {
        fprintf(stderr, "couldn't create data JSON object for IDENTIFY payload\n");
        return NULL;
    }

    properties = json_object();
    if (properties == NULL) {
        fprintf(stderr, "couldn't create properties JSON object for IDENTIFY payload\n");
        return NULL;
    }

    presence = json_object();
    if(presence == NULL) {
        fprintf(stderr, "couldn't create presence JSON object for IDENTIFY payload\n");
    }

    game = json_object();
    if(game == NULL) {
        fprintf(stderr, "couldn't create game JSON object for IDENTIFY payload\n");
    }

    json_object_set_new(payload, "op", json_integer(LD_OPCODE_IDENTIFY));
    json_object_set(payload, "d", data);

    json_object_set_new(data, "token", json_string(sd->bot_token));
    json_object_set_new(data, "compress", json_boolean(0));
    json_object_set_new(data, "large_threshold", json_integer(250));
    json_object_set(data, "properties", properties);
    json_object_set(data, "presence", presence);

    json_object_set_new(properties, "$os", json_string("linux"));
    json_object_set_new(properties, "$browser", json_string(lv));
    json_object_set_new(properties, "$device", json_string(lv));
    json_object_set_new(properties, "$referrer", json_string(""));
    json_object_set_new(properties, "$referring_domain", json_string(""));

    json_object_set_new(presence, "status", json_string("online"));
    json_object_set_new(presence, "since", json_null());
    json_object_set_new(presence, "afk", json_false());

    json_object_set_new(game, "name", json_string(sd->current_game));

    return payload;
}

const char * ld_get_gateway() {
    /*
     * makes a GET /gateway request to the base discord API endpoint
     */
//    printf("GET /gateway to %s\n", LD_BASE_URL);

    struct _u_map url_params;
    struct _u_request req;
    struct _u_response resp;

    char *URL;

    int res;
    char *response_body;

    json_t *response, *url;
    json_error_t error;

    if(u_map_init(&url_params) != U_OK) {
        perror("could not initialize _u_map url_params");
        return NULL;
    }

    if(ulfius_init_request(&req) != U_OK) {
        perror("could not initialize _u_requet req");
        return NULL;
    }

    if(ulfius_init_response(&resp) != U_OK) {
        perror("could not initialize _u_response resp");
    }

    URL = malloc(strlen(LD_BASE_URL) + strlen("/gateway") + 1);

    URL = strcpy(URL, LD_BASE_URL);
    URL = strcat(URL, "/gateway");

    req.http_verb = strdup("GET");
    req.http_url = strdup(URL);
    req.timeout = 20;

    res = ulfius_send_http_request(&req, &resp);
    if(res != U_OK) {
        perror("http request not sent");
        return NULL;
    }

    if (resp.status != 200) {
        fprintf(stderr, "Discord returned non-200 HTTP status code %li", resp.status);
        return NULL;
    }

    response_body = malloc((resp.binary_body_length + 1) * sizeof(char));
    strncpy(response_body, resp.binary_body, resp.binary_body_length);
    response_body[resp.binary_body_length] = '\0';

    response = json_loads(response_body, 0, &error);

    if(response == NULL) {
        fprintf(stderr, "JSON error on line %d: %s\n", error.line, error.text);
        return NULL;
    }

    if(!json_is_object(response)) {
        fprintf(stderr, "JSON error: response was not a JSON object\n");
        return NULL;
    }

    url = json_object_get(response, "url");

    if(url == NULL) {
        fprintf(stderr, "JSON error: could not find key \"url\" in response object\n");
        return NULL;
    }

    if(!json_string_value(url)) {
        fprintf(stderr, "JSON error: value for \"url\" is not a string\n");
        return NULL;
    }

    return json_string_value(url);
}

struct lws_context *ld_create_lws_context() {
    struct lws_context_creation_info info;
    struct lws_context *ret;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.extensions = exts;
    info.gid = -1;
    info.uid = -1;
    info.options = 0 | LWS_SERVER_OPTION_VALIDATE_UTF8 |
                   LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    ret = lws_create_context(&info);

    if(ret == NULL) {
        perror("couldn't create lws context");
    }

    return ret;

}

struct lws_client_connect_info *ld_create_lws_connect_info(struct lws_context *context, char *gateway_url) {
    struct lws_client_connect_info *i;
    i = malloc(sizeof(struct lws_client_connect_info));
    memset(i, 0, sizeof(struct lws_client_connect_info));
    char *ads_port;
    ads_port = malloc((strlen(gateway_url) + 6) * sizeof(char));
    sprintf(ads_port, "%s:%u", gateway_url, 443 & 65535);

    if(strspn(gateway_url, "wss://") == 6)
        i->address = strdup(gateway_url + 6);
    else
        i->address = strdup(gateway_url);

    i->context = context;
    i->port = 443;
    i->ssl_connection = 1;
    i->path = "/?v=" LD_DISCORDAPI_VERSION "&encoding=" LD_DISCORDAPI_WSENCODING;//"/?v=6&encoding=json"
    i->host = ads_port;
    i->origin = ads_port;
    i->protocol = protocols[0].name;

    return i;
}

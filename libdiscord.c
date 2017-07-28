//
// Created by dxing97 on 7/22/17.
//

#include <string.h>
#include <jansson.h>
#include "libdiscord.h"
#include <ulfius.h>

void ld_hbhandler(struct ld_sessiondata *sd) {
    //add heartbeat to send queue
    struct ld_sq_entry *entry;
    entry = malloc(sizeof(struct ld_sq_entry));
    entry->payload = ld_create_payload_heartbeat(sd->last_seq_num);
    TAILQ_INSERT_TAIL(sd->sq, entry, entries);

    //call socket_writable
    lws_callback_on_writable(sd->wsi);
}

struct ld_sessiondata *ld_create_sessiondata(struct ld_create_sessiondata_context *context) {
    //returns pointer to sd global variable

    memset(&sd, 0, sizeof(sd)); //set _everything_ to zero

    //init sendqueue
    TAILQ_INIT(sd.sq);
}

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

int ld_payload_hello_get_hb_interval(const char *payload) {
    json_t *decoded_payload, *data, *heartbeat_interval;
    json_error_t error;

    decoded_payload = json_loads(payload, 0, &error);

    data = json_object_get(decoded_payload, "d");
    if(data == NULL) {
        fprintf(stderr, "couldn't get heartbeat interval from HELLO: couldn't find d key");
        return -1;
    }

    heartbeat_interval = json_object_get(data, "heartbeat_interval");

    return (int) json_integer_value(heartbeat_interval);
}

int ld_payload_dispatch_get_seqnum(const char *payload) {
    json_t *decoded_payload, *seqnum;
    json_error_t error;

    decoded_payload = json_loads(payload, 0, &error);

    seqnum = json_object_get(decoded_payload, "s");
    if(seqnum == NULL) {
        fprintf(stderr, "couldn't get sequence number from DISPATCH: couldn't find s key");
        return -1;
    }

    return (int) json_integer_value(seqnum);
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

json_t * ld_create_payload_heartbeat(int sequence_number) {
    json_t *payload;
//    json_error_t error;

    payload = json_object();
    if(payload == NULL) {
        fprintf(stderr, "couldn't create payload JSON object for HEARTBEAT payload");
        return NULL;
    }

    json_object_set_new(payload, "op", json_integer(LD_OPCODE_HEARTBEAT));
    json_object_set_new(payload, "d", json_integer(sequence_number));

    return payload;
}

//
// Created by dxing97 on 7/22/17.
//

#include <string.h>
#include <jansson.h>
#include "libdiscord.h"
//#include <jansson.h>

enum ld_opcode ld_payloadbuf_get_opcode(char *payload, size_t len) {
    json_t *decoded_payload, *opcode;
    json_error_t json_error;
    size_t json_flags = 0, nlen; //no flags, see jansson documentation for possible decode flags
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

json_t* ld_create_payload_identify(const char bot_token[]) {
    json_t *payload, *data, *properties;

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
    /*
     * example:
{
    "op": 2,
    "d":
     {
        "token": "my_token",
        "properties": {
            "$os": "linux",
            "$browser": "my_library_name",
            "$device": "my_library_name",
            "$referrer": "",
            "$referring_domain": ""
        },
        "compress": true,
        "large_threshold": 250,
        "shard": [1, 10] //remove this if no sharding
     }
}
     */

    json_object_set_new(payload, "op", json_integer(LD_OPCODE_IDENTIFY));
    json_object_set(payload, "d", data);

    json_object_set_new(data, "token", json_string(bot_token));
    json_object_set_new(data, "compress", json_boolean(1));
    json_object_set_new(data, "large_threshold", json_integer(250));
    json_object_set(data, "properties", properties);

    json_object_set_new(properties, "$os", json_string("linux"));
    json_object_set_new(properties, "$browser", json_string("libdiscord"));
    json_object_set_new(properties, "$device", json_string("libdiscord"));
    json_object_set_new(properties, "$referrer", json_string(""));
    json_object_set_new(properties, "$referring_domain", json_string(""));

    return payload;


}
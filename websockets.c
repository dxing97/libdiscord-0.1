//
// Created by dxing97 on 7/27/17.
//

#include "websockets.h"

//FIX THIS MESS
int
callback_discord(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    json_error_t error;
    sd.wsd = (struct ld_wsdata *) user;
    int i = 0;

    switch (reason) { //why was the callback made?
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
            printf("peer initiated close, len: %lu\n", (unsigned long) len);
            for(i = 0; i < (int) len; i++) {
                printf(" %d: 0x%02X\n", i, (( unsigned char *)in)[i]);
            }
            printf("close code: %u\n", (unsigned int) (( unsigned char *)in)[0] << 8 | (( unsigned char *)in)[1]);
            break;
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (in != NULL) {
                fprintf(stderr, "couldn't make handshake with server: %.*s\nreason: %d\n",
                        (int) len, (char *) in, reason);
            }
            printf("server or client closed websocket\n");
            sd.ws_state = LD_WSSTATE_NOT_CONNECTED;
            force_exit = 1;
            break;
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Client has connected\n");
            sd.ws_state = LD_WSSTATE_CONNECTED_NOT_IDENTIFIED; //connected
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE: //recieved something from discord
            printf("RX: %s\n", json_dumps(json_loads((char *) in, 0, &error), JSON_INDENT(2)));

            sd.wsd->opcode = ld_payloadbuf_get_opcode(in, len);

            switch (sd.wsd->opcode) {
                case LD_OPCODE_DISPATCH:
                    sd.last_seq_num = ld_payload_dispatch_get_seqnum(in);
                    printf("recieved opcode DISPATCH with seqnum %d\n", sd.last_seq_num);
                    break;
                case LD_OPCODE_HEARTBEAT:
                    printf("recieved opcode HEARTBEAT(?)\n");
                    break;
                case LD_OPCODE_RECONNECT:
                    printf("recieved opcode RECONNECT\n");
                    break;
                case LD_OPCODE_INVALID_SESSION:
                    printf("recieved opcode INVALID_SESSION\n");
                    break;
                case LD_OPCODE_HELLO:
                    printf("recieved opcode HELLO, creating IDENTIFY payload\n");
                    //immediately send identify payload and begin sending heartbeats

                    //get the heartbeat interval
                    sd.heartbeat_interval = ld_payload_hello_get_hb_interval(in);

                    //create identify payload
                    i = sprintf((char *) &(sd.wsd->buf[LWS_PRE]), json_dumps(ld_create_payload_identify(&sd), 0));
                    if(i <= 0) {
                        fprintf(stderr, "couldn't write JSON payload to buffer");
                        return -1;
                    }
                    sd.wsd->len = (unsigned) i;
                    lws_callback_on_writable(wsi);
                    break;
                case LD_OPCODE_HEARTBEAT_AK:
                    printf("recieved opcode HEARTBEAT_AK\n");
                    break;
                default:
                    if(sd.wsd->opcode == -1) {
                        printf("payload containted no opcode!\n");
                    } else{
                        printf("recieved impossible opcode %d\n", sd.wsd->opcode);
                    }
                    return -1;
            }

            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("CLIENT_WRITABLE callback\n");
            if (sd.ws_state == LD_WSSTATE_SENDING_PAYLOAD) {
                printf("sending heartbeat\n");
                i = sprintf("%s", (char *) &(sd.wsd->buf[LWS_PRE]), json_dumps(ld_create_payload_heartbeat(sd.last_seq_num), 0));
                if(i <= 0) {
                    fprintf(stderr, "couldn't write JSON payload to buffer");
                    return -1;
                }
                sd.wsd->len = (unsigned) i;

                lwsl_notice("TX: %s\n", &sd.wsd->buf[LWS_PRE]);
                i = lws_write(wsi, &(sd.wsd->buf[LWS_PRE]), sd.wsd->len, LWS_WRITE_TEXT);
                if(i < 0) {
                    lwsl_err("ERROR %d writing to socket, hanging up\n", i);
                    return -1;
                }
                if(i < (int)sd.wsd->len) {
                    lwsl_err("Partial write\n");
                    return -1;
                }
                sd.ws_state = LD_WSSTATE_CONNECTED_IDENTIFIED;
                break;
            }
            lwsl_notice("TX: %s\n", &sd.wsd->buf[LWS_PRE]);
            i = lws_write(wsi, &(sd.wsd->buf[LWS_PRE]), sd.wsd->len, LWS_WRITE_TEXT);
            if(i < 0) {
                lwsl_err("ERROR %d writing to socket, hanging up\n", i);
                return -1;
            }
            if(i < (int)sd.wsd->len) {
                lwsl_err("Partial write\n");
                return -1;
            }
            break;

        default:
//            if(reason != LWS_CALLBACK_GET_THREAD_ID)
//            printf("unused callback: %d\n", reason);
            break;
    }
    return 0;

}

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

struct lws_extension exts[] = {
        {
                "permessage-deflate", //built in extension - RFC7629 compression extension
                lws_extension_callback_pm_deflate,
                "permessage-deflate; client_no_context_takeover; client_max_window_bits"
        },
        { NULL, NULL, NULL /* terminator */ }
};


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
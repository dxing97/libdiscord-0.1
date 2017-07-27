//get gateway for websocket
//connect to websocket
//print whatever comes through the websocket. DON'T SEND ANYTHING. Just print what comes out.
#include <ulfius.h>
#include <string.h>

#include <signal.h>

#include <libwebsockets.h>

#include <syslog.h>

#include "libdiscord.h"

static int force_exit = 0; //state - 0 - not connected

void sighandler(int sig) //handle SIGINT for graceful closure of the socket
{
	force_exit = 1;
}

struct ld_sessiondata sd;

int
callback_discord(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    json_error_t error;
    sd.wsd = (struct ld_wsdata *) user;
    int i = 0;

    switch (reason) { //why was the callback made?
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
            printf("peer initiated close, len: %lu\n", (unsigned long) len);
            for(i = 0; i < (int) len; i++) {
                printf(" %d: 0x%02X\n", i, (( unsigned char *)in)[i]);
            }
            printf("close code: %u\n", (( unsigned char *)in)[0] << 8 | (( unsigned char *)in)[1]);
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
            if (sd.ws_state == LD_WSSTATE_SENDING_HEARTBEAT) {
                printf("sending heartbeat\n");
                i = sprintf((char *) &(sd.wsd->buf[LWS_PRE]), json_dumps(ld_create_payload_heartbeat(sd.last_seq_num), 0));
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

int main (int argc, char **argv[] ) {
    struct timeval tv; //for heartbeat loop

    memset(&sd, 0, sizeof(sd));
    sd.bot_token = (char *) malloc(64*sizeof(char));

    printf("enter bot token:");
    fscanf(stdin, "%s", sd.bot_token);
    printf("%s\n", sd.bot_token);

    sd.current_game = malloc(129 * sizeof(char));
    sprintf(sd.current_game, LD_NAME "v %d%d.%d", LD_VERSION_MAJOR, LD_VERSION_MINOR, LD_VERSION_PATCH);

    char *gateway_url;
    gateway_url = strdup(ld_get_gateway());

    if (gateway_url == NULL) {
        perror("error getting gateway URL");
        return -1;
    }

    gateway_url = gateway_url + 6; //constant for "wss://"

    printf("websocket url: %s\n", gateway_url);
    sd.gateway_url = strdup(gateway_url);

    int debug_level =63,
        syslog_options = LOG_PID | LOG_PERROR;
    struct lws_context                  *context;
    struct lws_client_connect_info      *i;
    struct lws                          *wsi;

    memset(&i, 0, sizeof(i));

    lwsl_notice("libdiscord websocket startup\n");
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("lwsts", syslog_options, LOG_DAEMON);
    lws_set_log_level(debug_level, lwsl_emit_syslog);


    context = ld_create_lws_context();

    signal(SIGINT, sighandler);

    if(gettimeofday(&tv, NULL) == -1) {
        perror("can't get the time");
        goto bail;
    }



    while (!force_exit) {
        if (sd.ws_state == 0) {
            sd.ws_state = LD_WSSTATE_CONNECTING;
            lwsl_notice("connecting to %s\n", gateway_url);

            i = ld_create_lws_connect_info(context, gateway_url);

            printf("client connecting...\n");
            wsi = lws_client_connect_via_info(i);
            if (!wsi) {
                fprintf(stderr, "failed to connect to %s\n", gateway_url);
                goto bail;
            }
            sd.last_heartbeat = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            sd.heartbeat_interval = 5000; //wait 5 seconds after connecting to the gateway before sending the first heartbeat
        }
        if(sd.ws_state == LD_WSSTATE_CONNECTED_IDENTIFIED) {
            gettimeofday(&tv, NULL);
            if (sd.first_heartbeat == 0) {
                sd.ws_state = LD_WSSTATE_SENDING_HEARTBEAT;
                sd.last_heartbeat =  tv.tv_sec * 1000 + tv.tv_usec / 1000;
                sd.first_heartbeat = 1;
                lws_callback_on_writable(wsi);
            }
            if(( tv.tv_sec * 1000 + tv.tv_usec / 1000) - (sd.last_heartbeat) > sd.heartbeat_interval ) {
                //send a heartpeat payload
                //set the sd to sending_heartbeat
                sd.ws_state = LD_WSSTATE_SENDING_HEARTBEAT;
                sd.last_heartbeat =  tv.tv_sec * 1000 + tv.tv_usec / 1000;

                lws_callback_on_writable(wsi);
            }
        }

        lws_service(context, 10);
    }

bail:
    lws_context_destroy(context);
    lwsl_notice("discord.c might have exited cleanly.\n");


    return 0;
}
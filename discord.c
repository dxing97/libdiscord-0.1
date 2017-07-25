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
                printf(" %d: 0x%02X\n", i, ((unsigned int *)in)[i]);
            }
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
            sd.wsd->index = 0;
            sd.wsd->len = -1;
            sd.ws_state = LD_WSSTATE_CONNECTED_NOT_IDENTIFIED; //connected
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE: //recieved something from discord
            printf("RX: %s\n", json_dumps(json_loads((char *) in, 0, &error), JSON_INDENT(2)));

            sd.wsd->opcode = ld_payloadbuf_get_opcode(in, len);

            switch (sd.wsd->opcode) {
                case LD_OPCODE_DISPATCH:
                    printf("recieved opcode DISPATCH\n");
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
                    break;
            }

            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("CLIENT_WRITABLE callback\n");
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
            if(reason != LWS_CALLBACK_GET_THREAD_ID)
            printf("unused callback: %d\n", reason);
            break;
    }
    return 0;

}

//struct lws_protocols protocols[] = { //array of protocols that can be used
//	/* first protocol must always be HTTP handler */
//
//	{
//		"Discordv6",		/* name of the protocol- can be overridden with -e */
//		callback_discord, //the callback function for this protocol
//		sizeof(struct ld_wsdata),
//		/* per_session_data_size - total memory allocated for a connection, allocated on connection establishment and freed on connection takedown */
//		//note that this memory is given to the callback to work with
//		MAX_DISCORD_PAYLOAD, //rx_buffer_size - space for rx data (duh)
//		//id (unsigned int), - ignored by lws, can be used by user code for things like protocol version
//		//user (void *), - ignored by lws, can be used by user code for things and stuff
//		//tx_packet_size - 	space for tx data (duh)
//						//	0 means set it to the same size as rx_buffer_size,
//						//	otherwise tx data is sent in chunks of this size, which can be undesireable, so make sure the size is appropriate
//
//	},
//	{
//		NULL, NULL, 0		/* End of list */
//	}
//};


int main (int argc, char **argv[] ) {

    int pid = 1;

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

    while (!force_exit ) {
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
//            ((struct ld_wsdata *)(lws_get_protocol(wsi)->user))->token = bot_token;
            
        }
        //callback here?
//        pid = fork();//DO NOT FORK HERE IN THE LOOP. THAT IS A BAD IDEA.

        if (pid == 0) {
            //this is the child

        } else {
            //this is the parent, the child process as a PID of pid
            lws_service(context, 20000);
        }


    }

bail:
    lws_context_destroy(context);
    lwsl_notice("discord.c might have exited cleanly.\n");


    return 0;
}
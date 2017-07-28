//get gateway for websocket
//connect to websocket
//print whatever comes through the websocket. DON'T SEND ANYTHING. Just print what comes out.
#include <ulfius.h>
#include <string.h>

#include <signal.h>

#include <libwebsockets.h>

#include <syslog.h>

#include "libdiscord.h"

//struct ld_sessiondata sd;

int force_exit = 0; //state - 0 - not connected

void sighandler(int sig) //handle SIGINT for graceful closure of the socket
{
	force_exit = 1;
}

int main (int argc, char *argv[] ) {
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

    signal(SIGALRM, ld_hbhandler);

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
                sd.ws_state = LD_WSSTATE_SENDING_PAYLOAD;
                sd.last_heartbeat =  tv.tv_sec * 1000 + tv.tv_usec / 1000;
                sd.first_heartbeat = 1;
                lws_callback_on_writable(wsi);
            } else {
                alarm((unsigned int) (sd.heartbeat_interval/1000));
            }
            if(( tv.tv_sec * 1000 + tv.tv_usec / 1000) - (sd.last_heartbeat) > sd.heartbeat_interval ) {
                //send a heartpeat payload
                //set the sd to sending_heartbeat
                sd.ws_state = LD_WSSTATE_SENDING_PAYLOAD;
                sd.last_heartbeat =  tv.tv_sec * 1000 + tv.tv_usec / 1000;

                lws_callback_on_writable(wsi);
            }
        }

        lws_service(context, 50);
    }

bail:
    lws_context_destroy(context);
    lwsl_notice("discord.c might have exited cleanly.\n");


    return 0;
}
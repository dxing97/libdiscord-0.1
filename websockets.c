//
// Created by dxing97 on 7/27/17.
//

#include "websockets.h"
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

const struct lws_extension exts[] = {
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
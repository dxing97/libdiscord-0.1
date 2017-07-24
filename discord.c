//get gateway for websocket
//connect to websocket
//print whatever comes through the websocket. DON'T SEND ANYTHING. Just print what comes out.
#include <ulfius.h>
#include <string.h>

#include <signal.h>

#include <libwebsockets.h>

#include <syslog.h>

#include "libdiscord.h"

static int force_exit = 0, state = 0; //state - 0 - not connected
const char DISCORD_BASE_URL[] = "https://discordapp.com/api/v6";
const int port = 443; //WSS/HTTPS default port
#define URL_PARAMETERS "/?v=6&encoding=json"
const char bot_token[] = "MzM5MTg5MjkwMjIxMTc0Nzg0.DFgWNg.Ky2suLepztcTPReFUXaw93aoPOs";

struct per_session_data_discord *psd;

void sighandler(int sig) //handle SIGINT for graceful closure of the socket
{
	force_exit = 1;
}

static const struct lws_extension exts[] = {
	{
		"permessage-deflate", //built in extension - RFC7629 compression extension
		lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover; client_max_window_bits"
	},
	{ NULL, NULL, NULL /* terminator */ }
};

static int
callback_discord(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len) {
    struct per_session_data__discord *psd = (struct per_session_data__discord *) user;
    int n;
//    if (reason == LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER) {
        char **p;
    char *fields;
//    }


    switch (reason) { //why was the callback made?
        case LWS_CALLBACK_ESTABLISHED:
            printf("somehow accepted connection from a client?\n");
            break;
        case LWS_CALLBACK_SERVER_WRITEABLE:
            printf("SERVER_WRITABLE callback\n");
            break;
        case LWS_CALLBACK_RECEIVE: //server only
            psd->final = lws_is_final_fragment(wsi);
            psd->binary = lws_frame_is_binary(wsi);
            printf("CALLBACK_RECIEVE\n");

            break;
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (in != NULL) {
                fprintf(stderr, "couldn't make handshake with server: %.*s\nreason: %d\n",
                        (int) len, (char *) in, reason);
            }
            printf("server closed websocket\n");
            state = 0;
            force_exit = 1;
            break;
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("Client has connected\n");
            psd->index = 0;
            psd->len = -1;
            state = 2; //connected
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            //oboy something came from the server
            /*
             * use jansson to process the JSON format
             * use a case statement for the op code
             *
             */
            printf("RX: %s\n", (char*) in);

            psd->opcode = ld_payloadbuf_get_opcode(in, len);

            switch (psd->opcode) {
                case LD_OPCODE_NO_OP:
                    printf("recieved data contained no opcode!\n");
                    break;
                case LD_OPCODE_DISPATCH:
                    /*
                     * sent by: gateway
                     * when: new events occur
                     *
                     */
                    printf("recieved opcode DISPATCH\n");
                    break;
                case LD_OPCODE_HEARTBEAT:
                    /*
                     * sent by: client
                     * when: the interval specified in the heartbeat_interval in the hello payload has passed since the
                     * last heartbeat
                     */
                    printf("recieved opcode HEARTBEAT(?)\n");
                    break;
                case LD_OPCODE_IDENTIFY:
                    printf("recieved opcode IDENTIFY\n");
                    break;
                case LD_OPCODE_STATUS_UPDATE:
                    printf("recieved opcode STATUS_UPDATE\n");
                    break;
                case LD_OPCODE_VOICE_STATE_UPDATE:
                    printf("recieved opcode VOICE_STATUS_UPDATE\n");
                    break;
                case LD_OPCODE_VOICE_SERVER_PING:
                    printf("recieved opcode VOICE_SERVER_PING\n");
                    break;
                case LD_OPCODE_RESUME:
                    printf("recieved opcode RESUME\n");
                    break;
                case LD_OPCODE_RECONNECT:
                    printf("recieved opcode RECONNECT\n");
                    break;
                case LD_OPCODE_REQUEST_GUILD_MEMBERS:
                    printf("recieved opcode REQUEST_GUILD_MEMBERS\n");
                    break;
                case LD_OPCODE_INVALID_SESSION:
                    printf("recieved opcode INVALID_SESSION\n");
                    break;
                case LD_OPCODE_HELLO:
                    /*
                     * sent by: gateway
                     * when: client connects to the gateway
                     *
                     * immediately send opcode IDENTIFY with correct informations
                     */
                    printf("recieved opcode HELLO\n");
                    psd->opcode = LD_OPCODE_IDENTIFY;
                    psd->tx_data = ld_create_payload_identify(bot_token);
                    //lws_callback_on_writable(wsi);
                    break;
                case LD_OPCODE_HEARTBEAT_AK:
                    printf("recieved opcode HEARTBEAT_AK\n");
                    break;
                default:
                    break;
            }

            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("CLIENT_WRITABLE callback\n");

            psd->len = (unsigned int) sprintf(
                                (char *)&psd->buf[LWS_PRE], //put our message after LWS_PRE
                                json_string_value(psd->tx_data));

            lwsl_notice("Client TX: %s", &psd->buf[LWS_PRE]);
            n = lws_write(wsi, &psd->buf[LWS_PRE], psd->len, LWS_WRITE_TEXT); //write the message

            if (n < 0) { //if couldn't send message
                lwsl_err("ERROR %d writing to socket, hanging up\n", n);
                return -1;
            }
            if (n < (int)psd->len) { //only part of the message was sent! the rest should be sent automagically later
                lwsl_err("Partial write\n");
                return -1;
            }
            state = 4;

            break;
//        case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:break;
//        case LWS_CALLBACK_CLOSED_HTTP:break;
//        case LWS_CALLBACK_RECEIVE_PONG:break;
//        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:break;
//        case LWS_CALLBACK_HTTP:break;
//        case LWS_CALLBACK_HTTP_BODY:break;
//        case LWS_CALLBACK_HTTP_BODY_COMPLETION:break;
//        case LWS_CALLBACK_HTTP_FILE_COMPLETION:break;
//        case LWS_CALLBACK_HTTP_WRITEABLE:break;
//        case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:break;
//        case LWS_CALLBACK_FILTER_HTTP_CONNECTION:break;
//        case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:break;
//        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:break;
//        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:break;
//        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:break;
//        case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:break;
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
//            p = (char **)in;
//
//            if (len < 100)
//                return 1;
////            malloc(sizeof(""))
//
//            *p += sprintf(*p, "Authorization: Bot MjEzMDg0NzIyMTIzNzY3ODA5.DEgfXA.Xrd6EodEEqefOvVVYJVuDsz7RYo");

            return 0;

            break;
//        case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:break;
//        case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:break;
//        case LWS_CALLBACK_PROTOCOL_INIT:break;
//        case LWS_CALLBACK_PROTOCOL_DESTROY:break;
//        case LWS_CALLBACK_WSI_CREATE:break;
//        case LWS_CALLBACK_WSI_DESTROY:break;
        case LWS_CALLBACK_GET_THREAD_ID:
//            printf("pthread: %d\n", (unsigned int) pthread_self());
//            return (int) pthread_self();
            return 1;
            break;
//        case LWS_CALLBACK_ADD_POLL_FD:break;
//        case LWS_CALLBACK_DEL_POLL_FD:break;
//        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:break;
//        case LWS_CALLBACK_LOCK_POLL:break;
//        case LWS_CALLBACK_UNLOCK_POLL:break;
//        case LWS_CALLBACK_OPENSSL_CONTEXT_REQUIRES_PRIVATE_KEY:break;
//        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:break;
//        case LWS_CALLBACK_WS_EXT_DEFAULTS:break;
//        case LWS_CALLBACK_CGI:break;
//        case LWS_CALLBACK_CGI_TERMINATED:break;
//        case LWS_CALLBACK_CGI_STDIN_DATA:break;
//        case LWS_CALLBACK_CGI_STDIN_COMPLETED:break;
//        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:break;
//        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:break;
//        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:break;
//        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:break;
//        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:break;
//        case LWS_CALLBACK_HTTP_BIND_PROTOCOL:break;
//        case LWS_CALLBACK_HTTP_DROP_PROTOCOL:break;
//        case LWS_CALLBACK_CHECK_ACCESS_RIGHTS:break;
//        case LWS_CALLBACK_PROCESS_HTML:break;
//        case LWS_CALLBACK_ADD_HEADERS:break;
//        case LWS_CALLBACK_SESSION_INFO:break;
//        case LWS_CALLBACK_GS_EVENT:break;
//        case LWS_CALLBACK_HTTP_PMO:break;
//        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:break;
//        case LWS_CALLBACK_OPENSSL_PERFORM_SERVER_CERT_VERIFICATION:break;
//        case LWS_CALLBACK_RAW_RX:break;
//        case LWS_CALLBACK_RAW_CLOSE:break;
//        case LWS_CALLBACK_RAW_WRITEABLE:break;
//        case LWS_CALLBACK_RAW_ADOPT:break;
//        case LWS_CALLBACK_RAW_ADOPT_FILE:break;
//        case LWS_CALLBACK_RAW_RX_FILE:break;
//        case LWS_CALLBACK_RAW_WRITEABLE_FILE:break;
//        case LWS_CALLBACK_RAW_CLOSE_FILE:break;
//        case LWS_CALLBACK_SSL_INFO:break;
//        case LWS_CALLBACK_CHILD_WRITE_VIA_PARENT:break;
//        case LWS_CALLBACK_CHILD_CLOSING:break;
//        case LWS_CALLBACK_USER:break;

        default:
            if(reason != LWS_CALLBACK_GET_THREAD_ID)
            printf("unused callback: %d\n", reason);
            break;
    }
    return 0;
    
}

static struct lws_protocols protocols[] = { //array of protocols that can be used
	/* first protocol must always be HTTP handler */

	{
		"Discordv6",		/* name of the protocol- can be overridden with -e */
		callback_discord, //the callback function for this protocol
		sizeof(struct per_session_data__discord),	
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

char * print_map(const struct _u_map * map) {
  char * line, * to_return = NULL;
  const char **keys;
  int len, i;
  if (map != NULL) {
    keys = u_map_enum_keys(map);
    for (i=0; keys[i] != NULL; i++) {
      len = snprintf(NULL, 0, "key is %s, value is %s\n", keys[i], u_map_get(map, keys[i]));
      line = o_malloc((len+1)*sizeof(char));
      snprintf(line, (len+1), "key is %s, value is %s\n", keys[i], u_map_get(map, keys[i]));
      if (to_return != NULL) {
        len = strlen(to_return) + strlen(line) + 1;
        to_return = o_realloc(to_return, (len+1)*sizeof(char));
      } else {
        to_return = o_malloc((strlen(line) + 1)*sizeof(char));
        to_return[0] = 0;
      }
      strcat(to_return, line);
      o_free(line);
    }
    return to_return;
  } else {
    return NULL;
  }
}

void print_response(struct _u_response * response) {
  if (response != NULL) {
    char * headers = print_map(response->map_header);
    char response_body[response->binary_body_length + 1];
    strncpy(response_body, response->binary_body, response->binary_body_length);
    response_body[response->binary_body_length] = '\0';
    printf("protocol is\n%s\n\n  headers are \n%s\n\n  body is \n%s\n\n",
           response->protocol, headers, response_body);
    o_free(headers);
  }
}


int main (int argc, char **argv[] ) {

    //BEGIN: ulfius

    struct _u_map url_params;       //a _u_map for request header info
    struct _u_request req;          //HTTP request information
    struct _u_response response;    //HTTP response information
    char *uri = "/gateway/bot";
    //need to specify Discord gateway version and encoding (JSON)
    char botToken[] = "Bot MzM5MTg5MjkwMjIxMTc0Nzg0.DFgWNg.Ky2suLepztcTPReFUXaw93aoPOs";

    char url_prefix[strlen(DISCORD_BASE_URL) + strlen(uri)]; //URI to be queried
    strcpy(url_prefix, DISCORD_BASE_URL);
    strcat(url_prefix, uri);

    //printf("URL to send GET:\n");
    //fscanf(stdin, "%s", url_prefix);
    printf("%s\n", url_prefix);

    if (U_OK != u_map_init(&url_params)) { //initialize u_map for parameters for HTTP request header
        perror("could not initialize _u_map url_params!");
        exit(-1);
    }

    if (U_OK != ulfius_init_request(&req)) { //initialize parameters for
        perror("could not initialize _u_request req!");
        exit(-1);
    }

    req.http_verb = strdup("GET"); //make a get request
    req.http_url = strdup((url_prefix)); //to the discord gateway API endpoint
    req.timeout = 0; //timeout in seconds(?)
    u_map_put(&url_params,  "Authorization", 
                            botToken);
    u_map_put(&url_params,  "User-Agent", 
                            "DiscordBot (libdiscord, v0.0.1)");
    u_map_copy_into(req.map_header, &url_params);

    int res; //ulfius response code
    char *response_body; //the text in the response
    ulfius_init_response(&response); //intialize the response
    res = ulfius_send_http_request(&req, &response); //send the request
    if (res == U_OK) {
        print_response(&response); 
        //char response_body[response.binary_body_length + 1];
        response_body = (char *) malloc((response.binary_body_length + 1) * sizeof(char));
        strncpy(response_body, response.binary_body, response.binary_body_length);
        response_body[response.binary_body_length] = '\0';
        printf("binary response body raw: %s\n", response_body); }
    else {
        printf("Error in request: error");
    }
    
    //END: ulfius, BEGIN: jansson

    json_t *jroot;
    json_error_t error;
    
    jroot = json_loads(response_body, 0, &error); //decodes JSON string into json_t *jroot
    if(jroot == NULL) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        exit(-2);
    }

    if(!json_is_object(jroot)) {
        fprintf(stderr, "error: jroot is not an object\n");
        json_decref(jroot);
        return -2;
    }
    

    json_t *url, *shards;
    url = json_object_get(jroot, "url");
    if(!json_is_string(url)) {
        fprintf(stderr, "error: url not string");
        json_decref(jroot);
        return -2;
    }

    shards = json_object_get(jroot, "shards");
    if (!json_is_integer(shards)) {
        fprintf(stderr, "error: shard number is not integer");
        json_decref(jroot);
        return -2;
    }
    char *websocket_url;
    websocket_url = (char *) malloc(strlen(json_string_value(url)) + 1);
    websocket_url = strcpy(websocket_url, json_string_value(url));

            //= strdup(); //remove "wss://"
//    char websocket_url[] = "echo.websocket.org";
    //websocket_url = strchr(websocket_url, '/') + 2;

//    printf("websocket url: %s\n", websocket_url);
    printf("shard number: %" JSON_INTEGER_FORMAT "\n", json_integer_value(shards));

    ulfius_clean_response(&response);
    printf("response cleaned\n");

    websocket_url = (char *) realloc(websocket_url, 1000);
//    strcat(websocket_url, "?v=6&encoding=json");
    websocket_url = websocket_url + 6; //constant for wss://

    printf("websocket url: %s\n", websocket_url);

    lwsl_notice("libdiscord websocket startup\n");

    int debug_level =63,
        use_ssl = 1,
        listen_port = 0,
        n = 0, //connection state
        rate_us = 250000,
        opts = 0| //opts = 0, LWS_SERVER_OPTION_... are defined in libwebsockets.h,  look there for all of the different options
               LWS_SERVER_OPTION_VALIDATE_UTF8 | //"check UTF-8 correctness"
               LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT,
        syslog_options = LOG_PID | LOG_PERROR;
    unsigned long long                  oldus;
    struct lws_context_creation_info    info;
    struct lws_context                  *context;
    struct lws_client_connect_info      i;
    struct lws                          *wsi;
    struct timeval                      tv;
    char                                ads_port[512];

    memset(&info, 0, sizeof(info));
    memset(&i, 0, sizeof(i));

    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("lwsts", syslog_options, LOG_DAEMON);

    lws_set_log_level(debug_level, lwsl_emit_syslog);

    listen_port = CONTEXT_PORT_NO_LISTEN;

    info.port = listen_port;
    info.iface = NULL;
    info.protocols = protocols;
    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;
    info.gid = -1;
	info.uid = -1;
    info.extensions = exts; //extensions, in this case the built-in compression extension, required for Discord
    info.options = (unsigned int) opts;

    context = lws_create_context(&info);
    if (context == NULL) {
        fprintf(stderr, "error creating context\n");
        return -1;
    }

    signal(SIGINT, sighandler);

    gettimeofday(&tv, NULL); //gives the current UNIX time in timeval struct tv, which contains both seconds since Epoch and useconds since last second
	oldus = ((unsigned long long)tv.tv_sec * 1000000) + tv.tv_usec; //current time in useconds

    while (n >= 0 && !force_exit ) {
        if (state == 0) { //not conneccted
            state = 1;//connecting
            lwsl_notice("connecting to %s:%u\n", websocket_url, port);

            sprintf(ads_port, "%s:%u", websocket_url, port & 65535);

            memset(&i, 0, sizeof(i));

            i.context = context;
            i.address = websocket_url;
            i.port = port;
            i.port = port;
            i.ssl_connection = use_ssl;
            i.path = URL_PARAMETERS;
            i.host = ads_port;
            i.origin = ads_port;
            i.protocol = protocols[0].name;

            printf("client connecting...\n");
            wsi = lws_client_connect_via_info(&i);
            if (!wsi) {
                fprintf(stderr, "failed to connect to %s:%u\n", websocket_url, port);
            }
//            ((struct per_session_data__discord *)(lws_get_protocol(wsi)->user))->token = bot_token;
            
        }

//        gettimeofday(&tv, NULL);
//        if (((((unsigned long long)tv.tv_sec * 1000000) + tv.tv_usec) - oldus) > rate_us && state != 4) {
            //callback for writability would go here
//            lwsl_notice("callow\n");
//            lws_callback_on_writable(wsi);
            oldus = ((unsigned long long)tv.tv_sec * 1000000) + tv.tv_usec;


        n = lws_service(context, 1000);
    }

bail:
    lws_context_destroy(context);
    lwsl_notice("discord.c exited cleanly.\n");


    return 0;
}
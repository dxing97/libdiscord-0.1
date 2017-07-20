/*
 * libwebsockets-test-echo
 *
 * Copyright (C) 2010-2016 Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.	So unlike the library itself, they are licensed
 * Public Domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include "../include/libwebsockets.h"
#include "../include/lws_config.h"

#ifndef _WIN32
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#else
#include "gettimeofday.h"
#include <process.h>
#endif

// #include <pthread.h>

static volatile int force_exit = 0;
static int versa, state;
static int times = -1;

#define LOCAL_RESOURCE_PATH "/libwebsockets-test-server"

#define MAX_ECHO_PAYLOAD 1024

struct per_session_data__echo {
	size_t rx, tx;
	unsigned char buf[LWS_PRE + MAX_ECHO_PAYLOAD];
	unsigned int len;
	unsigned int index;
	int final;
	int continuation;
	int binary;
};

static int
callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{ //callback for the echo protocol (unnamed unless specified in the options)
	struct per_session_data__echo *pss = //create per-session data and point it to user (in the protocol array)
			(struct per_session_data__echo *)user;
	int n; //lws write protocol, like send as binary, this isn't the end of the message, send HTTP stuff, etc.

	switch (reason) {

#ifndef LWS_NO_SERVER

	case LWS_CALLBACK_ESTABLISHED: //if running as server, server has established a connection with a client
		pss->index = 0;
		pss->len = -1;
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE: //if lws_callback_on_writable is called (server can write to socket), this reason will be sent (for servers)
do_tx:
		//BEGIN SERVER CODE
		n = LWS_WRITE_CONTINUATION; //continue a previous ws message
		if (!pss->continuation) { //if not a continuation
			if (pss->binary) //if binary
				n = LWS_WRITE_BINARY; //send a ws binary message
			else
				n = LWS_WRITE_TEXT; //send a ws text message
			//note that there really isn't a difference between binary data and text data except for that little bit
			pss->continuation = 1;
		}
		if (!pss->final) //if part not end of message
			n |= LWS_WRITE_NO_FIN; // then tell lws that this part isn't the end of the message
		lwsl_info("+++ test-echo: writing %d, with final %d\n", 
			  pss->len, pss->final);

		pss->tx += pss->len; //size to transmit is size len
		n = lws_write(wsi, &pss->buf[LWS_PRE], pss->len, n); 
		//writes to the websocket wsi the content of pss->buf, 
		//pss->buf begins with data of size LWS_PRE, for sending ws protocol data, 
		//then data of size pss->len, for the payload, 
		//using the protocol specified in n (HTTP data, binary data, not part of the end of the message, etc.)
		//this means the data you want to send goes _after_ LWS_PRE in pss->buf[]
		//and the size of pss->buf is going to be LWS_PRE + pss->len
		//return value (n) is -1 for fatal error (!) or number of bytes sent
		if (n < 0) { //fatal error!
			lwsl_err("ERROR %d writing to socket, hanging up\n", n);
			return 1;
		}
		if (n < (int)pss->len) { //not everything was sent! the rest will be sent later automagically. won't usually happen
			lwsl_err("Partial write\n");
			return -1;
		}
		pss->len = -1; //everything got sent
		if (pss->final) //if that was the last message,
			pss->continuation = 0; //no continuation?
		lws_rx_flow_control(wsi, 1); //"if the output of a server process becomes choked, this allows flow control for the input side"
		//0 - disable reading
		//1 - enable reading
		//finished writing, so we can recieve data now
		break;

	case LWS_CALLBACK_RECEIVE: //data just showed up on the websocket!
do_rx:
		pss->final = lws_is_final_fragment(wsi); //is this data is the last part of a message
		pss->binary = lws_frame_is_binary(wsi); //if we're interested in knowing if this is binary data, we find out here
		lwsl_info("+++ test-echo: RX len %ld final %ld, pss->len=%ld\n",
			  (long)len, (long)pss->final, (long)pss->len);

		memcpy(&pss->buf[LWS_PRE], in, len); //copy the recieved data to pss_>buf buffer starting at LWS_PRE (for header info and stuff)
		assert((int)pss->len == -1); //make sure that pss->len is -1 (nothing is being sent/going to be sent)
		pss->len = (unsigned int)len; //set length of the payload to be sent to be the length of the recieved message
		pss->rx += len; //size recieved is size len

		lws_rx_flow_control(wsi, 0); //"if the output of a server process becomes choked, this allows flow control for the input side"
		//0 - disable reading
		//1 - enable reading
		//we're gonna write data, so don't recieve any more data
		lws_callback_on_writable(wsi);
		//call callback when socket is writable again
		break;
#endif
		//END SERVER CODE
#ifndef LWS_NO_CLIENT
	/* when the callback is used for client operations --> */

	case LWS_CALLBACK_CLOSED: //websocket session is gonna end
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: //client connection didn't succeed!
		//there may be an error string at (void *)in of length len, which gets printed here
		if (in != NULL) {
			fprintf(stderr, "couldn't make handshake with server: %.*s\n",(int) len, (char *) in);
		}
		lwsl_debug("closed\n");
		state = 0;
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED: //handshake completed!
		lwsl_debug("Client has connected\n");
		pss->index = 0;
		pss->len = -1;
		state = 2; //connected
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE: //data has appeared on the socket from the server at *in with length len
#ifndef LWS_NO_SERVER
		if (versa) //what does versa do?
			goto do_rx; //reuse rx code from server block
#endif
		lwsl_notice("Client RX: %s", (char *)in);
		break;

	case LWS_CALLBACK_CLIENT_WRITEABLE: //socket is writable (this only appears when you call lws_callback_on_writable())
#ifndef LWS_NO_SERVER
		printf("socket writable!\n");
		if (versa) { //what does versa do?
			if (pss->len != (unsigned int)-1) //if there's something to send
				goto do_tx;
			break;
		}
#endif
		/* we will send our packet... */
		pss->len = sprintf((char *)&pss->buf[LWS_PRE], //put our message after LWS_PRE
				   "hello from libwebsockets-test-echo client pid %d index %d\n", //out message
				   getpid(), pss->index++); //PID and the nth message we've sent (index)
		lwsl_notice("Client TX: %s", &pss->buf[LWS_PRE]);
		n = lws_write(wsi, &pss->buf[LWS_PRE], pss->len, LWS_WRITE_TEXT); //write thesend the message
		if (n < 0) { //if couldn't send message
			lwsl_err("ERROR %d writing to socket, hanging up\n", n);
			return -1;
		}
		if (n < (int)pss->len) { //only part of the message was sent! the rest should be sent automagically later
			lwsl_err("Partial write\n");
			return -1;
		}
		break;
#endif
	case LWS_CALLBACK_GET_THREAD_ID: //snippet from test-server.c
		/*
		 * if you will call "libwebsocket_callback_on_writable"
		 * from a different thread, return the caller thread ID
		 * here so lws can use this information to work out if it
		 * should signal the poll() loop to exit and restart early
		 */

		// return pthread_getthreadid_np();
		// printf("thread thing: %d | %d\n", LWS_CALLBACK_GET_THREAD_ID, reason);
		// return -1;

		break;
	default: //reason for callback wasn't specified
		// printf("callback reason not accounted for, reason: %d\n", reason);
		break;
	}

	return 0;
}



static struct lws_protocols protocols[] = { //array of protocols that can be used
	/* first protocol must always be HTTP handler */

	{
		"",		/* name of the protocol- can be overridden with -e */
		callback_echo, //the callback function for this protocol
		sizeof(struct per_session_data__echo),	
		/* per_session_data_size - total memory allocated for a connection, allocated on connection establishment and freed on connection takedown */
		//note that this memory is given to the callback to work with
		MAX_ECHO_PAYLOAD, //rx_buffer_size - space for rx data (duh)
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


void sighandler(int sig) //handle SIGINT for graceful closure of the socket
{
	force_exit = 1;
}

static struct option options[] = { //getopt_long long options
	{ "help",	no_argument,		NULL, 'h' },
	{ "debug",	required_argument,	NULL, 'd' },
	{ "port",	required_argument,	NULL, 'p' },
	{ "ssl-cert",	required_argument,	NULL, 'C' },
	{ "ssl-key",	required_argument,	NULL, 'k' },
#ifndef LWS_NO_CLIENT
	{ "client",	required_argument,	NULL, 'c' },
	{ "ratems",	required_argument,	NULL, 'r' },
#endif
	{ "ssl",	no_argument,		NULL, 's' },
	{ "versa",	no_argument,		NULL, 'v' },
	{ "uri",	required_argument,	NULL, 'u' },
	{ "passphrase", required_argument,	NULL, 'P' },
	{ "interface",	required_argument,	NULL, 'i' },
	{ "times",	required_argument,	NULL, 'n' },
	{ "echogen",	no_argument,		NULL, 'e' },
#ifndef LWS_NO_DAEMONIZE
	{ "daemonize",	no_argument,		NULL, 'D' },
#endif
	{ NULL, 0, 0, 0 }
};

int main(int argc, char **argv)
{
	int n = 0;
	int port = 7681;
	int use_ssl = 0;
	struct lws_context *context;
	int opts = 0;
	char interface_name[128] = "";
	const char *_interface = NULL;
	char ssl_cert[256] = "cert.pem";
	char ssl_key[256] = "key.pem";
#ifndef _WIN32
/* LOG_PERROR is not POSIX standard, and may not be portable */
#ifdef __sun
	int syslog_options = LOG_PID;
#else
	int syslog_options = LOG_PID | LOG_PERROR;
#endif
#endif
	int client = 0;
	int listen_port = 80;
	struct lws_context_creation_info info;
	char passphrase[256];
	char uri[256] = "/";
#ifndef LWS_NO_CLIENT
	char address[256], ads_port[256 + 30];
	int rate_us = 250000;
	unsigned long long oldus;
	struct lws *wsi;
	int disallow_selfsigned = 0;
	struct timeval tv;
	const char *connect_protocol = NULL;
	struct lws_client_connect_info i;
#endif

	int debug_level = 15;
#ifndef LWS_NO_DAEMONIZE
	int daemonize = 0;
#endif

	memset(&info, 0, sizeof info);

#ifndef LWS_NO_CLIENT
	lwsl_notice("Built to support client operations\n");
#endif
#ifndef LWS_NO_SERVER
	lwsl_notice("Built to support server operations\n");
#endif

	while (n >= 0) {
		n = getopt_long(argc, argv, "i:hsp:d:DC:k:P:vu:n:e" "c:r:", options, NULL);
		// n is the option char, will be -1 when out of options to process
		// optarg is the arguments of the option chosen


		if (n < 0) //if no opt to process
			continue; //check condition to break loop
		switch (n) {
		case 'P': //SSL key passphrase, don't need, server only
			strncpy(passphrase, optarg, sizeof(passphrase));
			passphrase[sizeof(passphrase) - 1] = '\0';
			info.ssl_private_key_password = passphrase;
			break;
		case 'C': //SSL cert, don't need, server only
			strncpy(ssl_cert, optarg, sizeof(ssl_cert));
			ssl_cert[sizeof(ssl_cert) - 1] = '\0';
			disallow_selfsigned = 1;
			break;
		case 'k': //SSL key, don't need, server only
			strncpy(ssl_key, optarg, sizeof(ssl_key));
			ssl_key[sizeof(ssl_key) - 1] = '\0';
			break;
		case 'u': //URI to connect to/serve from, get from discord API and not command line
			strncpy(uri, optarg, sizeof(uri));
			uri[sizeof(uri) - 1] = '\0';
			break;

#ifndef LWS_NO_DAEMONIZE
		case 'D': //daemonize - run as background process, probably don't want (but leave it in)
			daemonize = 1;
#if !defined(_WIN32) && !defined(__sun)
			syslog_options &= ~LOG_PERROR;
#endif
			break;
#endif
// #ifndef LWS_NO_CLIENT
		case 'c': //run as client and connect to this address, get from discord API and not command line
			client = 1;
			strncpy(address, optarg, sizeof(address) - 1);
			address[sizeof(address) - 1] = '\0';
			port = 80; //change port based on gateway URL - probably 443 for WSS
			break;
		case 'r': //rate at which echos are sent
			rate_us = atoi(optarg) * 1000;
			break;
// #endif
		case 'd': //debug level to use
			debug_level = atoi(optarg);
			break;
		case 's': //use SSL as client
			use_ssl = 1; /* 1 = take care about cert verification, 2 = allow anything */
			break;
		case 'p': //port to use - get from discord API
			port = atoi(optarg);
			break;
		case 'v': // ??? what does versa do? something to do with echo protocol
			versa = 1;
			break;
		case 'e': //forcing to use echogen? remove.
			protocols[0].name = "lws-echogen";
			connect_protocol = protocols[0].name;
			lwsl_err("using lws-echogen\n");
			break;
		case 'i': //specify interface, don't need, just pick an available interface. 
			strncpy(interface_name, optarg, sizeof interface_name);
			interface_name[(sizeof interface_name) - 1] = '\0';
			_interface = interface_name;
			break;
		case 'n': //echo specific, don't need
			times = atoi(optarg);
			break;
		case '?': //help, modify this for discord
		case 'h':
			fprintf(stderr, "Usage: libwebsockets-test-echo\n"
				"  --debug	/ -d <debug bitfield>\n"
				"  --port	/ -p <port>\n"
				"  --ssl-cert	/ -C <cert path>\n"
				"  --ssl-key	/ -k <key path>\n"
#ifndef LWS_NO_CLIENT
				"  --client	/ -c <server IP>\n"
				"  --ratems	/ -r <rate in ms>\n"
#endif
				"  --ssl	/ -s\n"
				"  --passphrase / -P <passphrase>\n"
				"  --interface	/ -i <interface>\n"
				"  --uri	/ -u <uri path>\n"
				"  --times	/ -n <-1 unlimited or times to echo>\n"
#ifndef LWS_NO_DAEMONIZE
				"  --daemonize	/ -D\n"
#endif
			);
			exit(1);
		}
	}

#ifndef LWS_NO_DAEMONIZE
	/*
	 * normally lock path would be /var/lock/lwsts or similar, to
	 * simplify getting started without having to take care about
	 * permissions or running as root, set to /tmp/.lwsts-lock
	 */
#if defined(WIN32) || defined(_WIN32)
#else
	if (!client && daemonize && lws_daemonize("/tmp/.lwstecho-lock")) {
		fprintf(stderr, "Failed to daemonize\n");
		return 1;
	}
#endif
#endif

#ifndef _WIN32
	/* we will only try to log things according to our debug_level */
	//logging options
	setlogmask(LOG_UPTO (LLL_DEBUG));
	openlog("lwsts", syslog_options, LOG_DAEMON);
#endif

	/* tell the library what debug level to emit and to send it to syslog */
	lws_set_log_level(debug_level, lwsl_emit_syslog);

	lwsl_notice("libwebsockets test server echo - license LGPL2.1+SLE\n");
	lwsl_notice("(C) Copyright 2010-2016 Andy Green <andy@warmcat.com>\n");

// #ifndef LWS_NO_CLIENT
	if (client) { //client code
		lwsl_notice("Running in client mode\n");
		listen_port = CONTEXT_PORT_NO_LISTEN; //don't listen on any ports, use as client
		if (use_ssl && !disallow_selfsigned) { //self-signed is ok (I think?)
			lwsl_notice("allowing selfsigned\n");
			use_ssl = 2;
		} else {
			lwsl_notice("requiring server cert validation against %s\n", //check to see if ssl_cert is valid against our own cert, probably can't use
				  ssl_cert);
			info.ssl_ca_filepath = ssl_cert;
		}
	} else { //server code, don't need, remove
// #endif
// #ifndef LWS_NO_SERVER
		lwsl_notice("server mode disabled\n");
		exit(0);
// #endif
// #ifndef LWS_NO_CLIENT
	}
// #endif

	info.port = listen_port; //set to CONTEXT_PORT_NO_LISTEN because client mode
	info.iface = _interface; //NULL, unless specified in settings
	info.protocols = protocols; //protocols to be used, note that the protocol name is empty
	if (use_ssl && !client) {
		printf("using user defined SSL cert\n");
		info.ssl_cert_filepath = ssl_cert;
		info.ssl_private_key_filepath = ssl_key;
	} else
		if (use_ssl && client) {
			printf("client mode with no cert\n");
			info.ssl_cert_filepath = NULL;
			info.ssl_private_key_filepath = NULL;
		}
	info.gid = -1;
	info.uid = -1;
	info.extensions = exts; //extensions, in this case the built-in compression extension
	info.options = opts | //opts = 0, LWS_SERVER_OPTION_... are defined in libwebsockets.h,  look there for all of the different options
		LWS_SERVER_OPTION_VALIDATE_UTF8; //"check UTF-8 correctness"
	if (use_ssl)
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT; //"initialize the SSL library"
#ifndef LWS_NO_EXTENSIONS //is this section needed?
	info.extensions = exts; //check to see if LWS_NO_EXTENSIONS is true or false
#endif

	context = lws_create_context(&info); //create context for ws 
	if (context == NULL) { //make sure init suceeded 
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}


	signal(SIGINT, sighandler); //when SIGINIT (Ctrl-C) is passed to program, call sighandler, which gracefully closes the connection

#ifndef LWS_NO_CLIENT
	gettimeofday(&tv, NULL); //gives the current UNIX time in timeval struct tv, which contains both seconds since Epoch and useconds since last second
	oldus = ((unsigned long long)tv.tv_sec * 1000000) + tv.tv_usec; //current time in useconds
#endif

	n = 0;
	while (n >= 0 && !force_exit) { //force exit == 1 when SIGINT (Ctrl-C) is sent
		if (client && !state && times) {
			//client == 1 in client mode, 
			//state == 0 when connection closed (error or otherwise), 1 when connecting, 2 when connected
			//times - number of times to send echo, -1 for infinite
			state = 1;
			lwsl_notice("Client connecting to %s:%u....\n",
				    address, port);
			/* we are in client mode */

			address[sizeof(address) - 1] = '\0'; //same as the address case above
			sprintf(ads_port, "%s:%u", address, port & 65535); //append port to address, and make sure the port is valid
			if (times > 0) //if still need to connect, decrement times to send
				times--;

			memset(&i, 0, sizeof(i)); //set connect info to 0

			i.context = context; //context for connection
			i.address = address; //address to connect to
			i.port = port; //port to connect to
			use_ssl = LCCSCF_USE_SSL |
				  LCCSCF_ALLOW_SELFSIGNED |
				  LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
			i.ssl_connection = use_ssl; //whether or not to use SSL
			printf("value of use_ssl:%d\n", use_ssl);
			i.path = uri; //URI to connect to, in this case "/". use discord URL paramaters
			i.host = ads_port; //host to connect to
			i.origin = ads_port; //origin server
			i.protocol = connect_protocol; //protocols to present

			wsi = lws_client_connect_via_info(&i); //connect!
			if (!wsi) {
				lwsl_err("Client failed to connect to %s:%u\n",
					 address, port);
				goto bail;
			}
		}
		// printf("client: %d, versa: %d, times: %d\n", client, versa, times);
		if (client && !versa && times) { 
			//in connected or connecting mode, 
			//versa is zero,
			//times is nonzero (includes -1)
			gettimeofday(&tv, NULL);

			if (((((unsigned long long)tv.tv_sec * 1000000) + tv.tv_usec) - oldus) > rate_us) { 
				//if time since last call to gettimeofday
				//i.e. rate limiting
				printf("callback on writable called\n");
				lws_callback_on_writable(wsi);//, //request callback for these protocols, in this case the echo protocol
						/*&protocols[0]);*/
				oldus = ((unsigned long long)tv.tv_sec * 1000000) + tv.tv_usec; //set new oldus
				if (times > 0)
					times--;
			}
		}

		if (client && !state && !times) //if in client mode, not connected, and no remaining echos to send
			break; //end while loop and start closing the socket
		// printf("service\n");
		n = lws_service(context, 10); 
		//services pending websocket activity
		//if running as server, accepts new connections to the server
		//for client and server, calls "recieve callback for incoming frame data"
		//10 - timeout in ms, waits this long if there's nothing to do
		//if timeout is 0, returns immediately if there's nothing to do
		//can fork this to a new process with a large timeout to handle asynchronously and efficiently
	}
#ifndef LWS_NO_CLIENT
bail:
#endif
	lws_context_destroy(context);

	lwsl_notice("libwebsockets-test-echo exited cleanly\n");
#ifndef _WIN32
	closelog();
#endif

	return 0;
}

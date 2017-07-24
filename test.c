//
// Created by dxing97 on 7/21/17.
//

#include <libwebsockets.h>
//#include <stdio.h>
#include "libdiscord.h"

int main () {
//    char URI[] = "https://discordapp.com/api/v6/gateway/bot";
////    URI = (char *) malloc(sizeof(char) * 1000);
////    URI = strcpy(URI, "https://discordapp.com/api/v6/gateway/bot");
//    char prot[100], ads[100], path[100];
//
//    int *port;
//    lws_parse_uri(URI, &prot, &ads, port, &path);
//    printf("output: %s\n%s\n%d\n%s\n", prot, ads, *port, path);
    enum ld_opcode opcode;

    char payload[] = "{\n"
            "    \"op\": 3,\n"
            "    \"d\": {},\n"
            "    \"s\": 42,\n"
            "    \"t\": \"GATEWAY_EVENT_NAME\"\n"
            "}";

    printf("payload: \n%s\nlen: %d", payload, (int) strlen(payload));
    opcode = ld_payloadbuf_get_opcode(payload, strlen(payload) -5);
    printf("opcode: %d\n", opcode);
    return 0;

}
//
// Created by dxing97 on 7/21/17.
//

#include <libwebsockets.h>
//#include <stdio.h>
#include "libdiscord.h"

int main(int argc, char *argv[]) {
//    uv_loop_t *loop = malloc(sizeof(uv_loop_t));
//    uv_loop_init(loop);
//
//    printf("now quitting.\n");
//    uv_run(loop, UV_RUN_DEFAULT);
//
//    uv_loop_close(loop);
//    free(loop);
//    return 0;
    char *gateway_url;
    gateway_url = strdup(ld_get_gateway());

    if (gateway_url == NULL) {
        return -1;
    }

    printf("gateway URL: %s\n", gateway_url);

    return 0;


}


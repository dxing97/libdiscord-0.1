//
// Created by danielxing.4 on 8/10/2016.
//

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libwebsockets.h>
#include <curl/curl.h>

int main() {
    printf("Daxe");
    CURL *curl;
    //CURLcode res;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://discordapp.com/");

        curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }


    //curl_easy_cleanup();
    return 0;
}
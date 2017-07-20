#include <ulfius.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

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


int main (int argc, char *argv[]) {
  struct _u_map url_params;
  struct _u_request req;
  struct _u_response response;
  char url_prefix[] = "https://discordapp.com/api/gateway/bot";

  //printf("URL to send GET:\n");
  //fscanf(stdin, "%s", url_prefix);
  printf("%s\n", url_prefix);

  if (U_OK != u_map_init(&url_params)) {
    perror("could not initialize _u_map url_params!");
    exit(-1);
  }

  if (U_OK != ulfius_init_request(&req)) {
    perror("could not initialize _u_request req!");
    exit(-1);
  }

  req.http_verb = strdup("GET");
  req.http_url = strdup((url_prefix));
  req.timeout = 20;
  u_map_put(&url_params, "Authorization", "Bot MjEzMDg0NzIyMTIzNzY3ODA5.DEgfXA.Xrd6EodEEqefOvVVYJVuDsz7RYo");
  u_map_copy_into(req.map_header, &url_params);

  int res;
  char *response_body;
  ulfius_init_response(&response);
  res = ulfius_send_http_request(&req, &response);
  if (res == U_OK) {
    print_response(&response); 
    //char response_body[response.binary_body_length + 1];
    response_body = (char *) malloc((response.binary_body_length + 1) * sizeof(char));
    strncpy(response_body, response.binary_body, response.binary_body_length);
    response_body[response.binary_body_length] = '\0';
    printf("binary response body raw: %s\n", response_body); }
  else {
    printf("Error in request: error", res);
  }
  
  
  json_t *root;
  json_error_t error;
  
  root = json_loads(response_body, 0, &error);
  if(!root) {
    fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
    exit(-2);
  }

  if(!json_is_object(root)) {
    fprintf(stderr, "error: root is not an obect\n");
    json_decref(root);
    return -2;
  }
  

  json_t *url, *shards;
  url = json_object_get(root, "url");
  if(!json_is_string(url)) {
    fprintf(stderr, "error: url not string");
    json_decref(root);
    return -2;
  }

  shards = json_object_get(root, "shards");
  if (!json_is_integer(shards)) {
    fprintf(stderr, "error: shard number is not integer");
    json_decref(root);
    return -2;
  }

  printf("websocket url: %s\n", json_string_value(url));
  printf("shard number: %" JSON_INTEGER_FORMAT "\n", json_integer_value(shards));

  ulfius_clean_response(&response);
  printf("response cleaned\n");
  u_map_put(&url_params, "Upgrade", "websocket");
  u_map_put(&url_params, "Connection", "Upgrade");
  u_map_put(&url_params, "Sec-Websocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
  printf("creating uri for websocket\n");
  char url_string[json_string_length(url) + 1];
  strcpy(url_string, json_string_value(url));
  printf("1\n");
  strcpy(url_prefix, strcat(url_string, "/*/?v=5&encoding=json*/"));
  printf("url prefix: %s\n", url_prefix);
  req.http_url = strdup((url_prefix));
  u_map_copy_into(req.map_header, &url_params);
  res = ulfius_send_http_request(&req, &response);
  
  if (res == U_OK) {
    print_response(&response); 
    //char response_body[response.binary_body_length + 1];
    response_body = (char *) malloc((response.binary_body_length + 1) * sizeof(char));
    strncpy(response_body, response.binary_body, response.binary_body_length);
    response_body[response.binary_body_length] = '\0';
    printf("binary response body raw: %s\n", response_body); }
  else {
    printf("Error in request: error %d", res);
  }
  

  return 0;
}

#include <stdio.h>
#include "http_request.h"

void print_http_request(http_request_t *request) {
    int i;
    http_headers_t headers = request->headers;

    printf("path: %s\n", request->path);
    printf("method: %s\n", http_method_str(request->method));
    for (i = 0; i < headers.num_headers; i++) {
        printf("%s: %s\n", headers.field[i], headers.value[i]);
    }

    printf("body: \n%s\n", request->body);
    fflush(stdout);
}

int request_on_url_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return -1;

    http_request_t *request = (http_request_t *) parser->data;
    struct http_parser_url u;
    int is_connect = parser->method == HTTP_CONNECT;

    http_parser_url_init(&u);
    if (http_parser_parse_url(at, len, is_connect, &u) == 0) {
        if (u.field_set & (1 << UF_PATH)) {
            strncpy(request->path, at + u.field_data[UF_PATH].off,
                u.field_data[UF_PATH].len);
        }

        if (u.field_set & (1 << UF_SCHEMA)) {
            strncpy(request->schema, at + u.field_data[UF_SCHEMA].off,
                u.field_data[UF_SCHEMA].len);
        }

        if (u.field_set & (1 << UF_HOST)) {
            strncpy(request->host, at + u.field_data[UF_HOST].off,
                u.field_data[UF_HOST].len);
        }

        if (u.field_set & (1 << UF_PORT)) {
            strncpy(request->port, at + u.field_data[UF_PORT].off,
                u.field_data[UF_PORT].len);
        }
    }

    return 0;
}

int request_on_header_field_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return -1;

    http_request_t *request = (http_request_t *) parser->data;
    http_headers_t *headers = &request->headers;

    if (headers->last_header_element != HEADER_FIELD) {
        headers->num_headers++;
    }

    strncat(headers->field[headers->num_headers - 1], at, len);
    headers->last_header_element = HEADER_FIELD;

    return 0;
}

int request_on_header_value_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return -1;

    http_request_t *request = (http_request_t *) parser->data;
    http_headers_t *headers = &request->headers;
    strncat(headers->value[headers->num_headers - 1], at, len);
    headers->last_header_element = HEADER_VALUE;

    return 0;
}

int request_on_body_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return -1;

    http_request_t *request = (http_request_t *) parser->data;
    strncat(request->body, at, len);

    return 0;
}

int request_on_message_complete_cb(http_parser *parser) {
    if (!parser->data) return -1;

    http_request_t *request = (http_request_t *) parser->data;
    request->method = parser->method;
    request->on_message_completed = true;

    struct http_parser_url u;
    int is_connect = parser->method == HTTP_CONNECT;

    http_parser_url_init(&u);

    char url[64] = {0};

    char *host = find_header_value(&request->headers, "Host");

    if (host) {
        if (!strstr(host, "http")) {
            strcpy(url, "http://");
            strcat(url, host);
        } else {
            strcpy(url, host);
        }
    }

    if (strcmp(url, "") != 0
        && http_parser_parse_url(url, strlen(url), is_connect, &u) == 0) {
        if (u.field_set & (1 << UF_PATH)) {
            strncpy(request->path, url + u.field_data[UF_PATH].off,
                u.field_data[UF_PATH].len);
        }

        if (u.field_set & (1 << UF_SCHEMA)) {
            strncpy(request->schema, url + u.field_data[UF_SCHEMA].off,
                u.field_data[UF_SCHEMA].len);
        }

        if (u.field_set & (1 << UF_HOST)) {
            strncpy(request->host, url + u.field_data[UF_HOST].off,
                u.field_data[UF_HOST].len);
        }

        if (u.field_set & (1 << UF_PORT)) {
            strncpy(request->port, url + u.field_data[UF_PORT].off,
                u.field_data[UF_PORT].len);
        }
    }

    if (strcmp(request->path, "") == 0) {
        strcpy(request->path, "/");
    }

    if (request->path != NULL && strlen(request->path) != 1
        && request->path[strlen(request->path)-1] == '/') {
        request->path[strlen(request->path)-1] = '\0';
    }
    
    return 0;
}

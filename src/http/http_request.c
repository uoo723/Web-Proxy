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
    if (!parser->data) return 0;

    http_request_t *request = (http_request_t *) parser->data;
    struct http_parser_url u;
    int is_connect = parser->method == HTTP_CONNECT;

    http_parser_url_init(&u);
    if (http_parser_parse_url(at, len, is_connect, &u) == 0) {
        strncpy(request->path, at + u.field_data[UF_PATH].off,
            u.field_data[UF_PATH].len);
    }

    return 0;
}

int request_on_header_field_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return 0;

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
    if (!parser->data) return 0;

    http_request_t *request = (http_request_t *) parser->data;
    http_headers_t *headers = &request->headers;
    strncat(headers->value[headers->num_headers - 1], at, len);
    headers->last_header_element = HEADER_VALUE;

    return 0;
}

int request_on_body_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return 0;

    http_request_t *request = (http_request_t *) parser->data;
    strncat(request->body, at, len);

    return 0;
}

int request_on_message_complete_cb(http_parser *parser) {
    if (!parser->data) return 0;

    http_request_t *request = (http_request_t *) parser->data;
    request->method = parser->method;
    request->on_message_completed = 1;
    return 0;
}

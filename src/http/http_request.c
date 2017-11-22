#include <stdio.h>
#include <stdlib.h>
#include "http_request.h"

static __thread char *local_content;
static __thread size_t local_content_len;
static __thread size_t local_alloc_content_size;

void print_http_request(http_request_t *request) {
    int i;
    http_headers_t headers = request->headers;

    printf("path: %s\n", request->path);
    printf("method: %s\n", http_method_str(request->method));
    for (i = 0; i < headers.num_headers; i++) {
        printf("%s: %s\n", headers.field[i], headers.value[i]);
    }

    printf("content: \n%s\n", request->content);
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

    if (local_content_len == 0) {
        local_alloc_content_size = 2 * sizeof(char) * len;
        local_content = malloc(local_alloc_content_size);

        if (local_content == NULL) {
            return -1;
        }

    } else if (local_content_len != 0
        && (local_content_len + len) > local_alloc_content_size) {
            local_alloc_content_size += 2 * sizeof(char) * len;
            local_content = realloc(local_content, local_alloc_content_size);

            if (local_content == NULL) {
                return -1;
            }
    }

    memcpy(local_content + local_content_len, at, len);
    local_content_len += len;
    return 0;
}

int request_on_message_complete_cb(http_parser *parser) {
    if (!parser->data) return -1;

    http_request_t *request = (http_request_t *) parser->data;
    request->method = parser->method;
    request->on_message_completed = true;
    request->http_major = parser->http_major;
    request->http_minor = parser->http_minor;

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

    if (request->port == NULL || strcmp(request->port, "") == 0) {
        strcpy(request->port, "80");
    }

    if (local_content_len != 0) {
        request->content = local_content;
        request->content_length = local_content_len;
        local_content = NULL;
        local_content_len = 0;
        local_alloc_content_size = 0;
    }

    return 0;
}

bool make_request_string(http_request_t *request, char **dst, size_t *dst_size) {
    http_headers_t *headers;
    size_t buf_size;
    char *buf;
    int i;

    headers = &request->headers;
    buf_size = request->content_length + 2048;
    *dst = NULL;
    *dst_size = 0;
    buf = malloc(buf_size);

    if (!buf) {
        return false;
    }

    memset(buf, 0, buf_size);

    sprintf(buf, "%s %s HTTP/%d.%d\r\n", http_method_str(request->method),
        request->path, request->http_major, request->http_minor);

    for (i = 0; i < headers->num_headers; i++) {
        if (strlen(buf) + 1 > buf_size) {
            buf_size += 1024;
            buf = realloc(buf, buf_size);
            if (!buf) {
                return false;
            }
        }

        strcat(buf, headers->field[i]);
        strcat(buf, ": ");
        strcat(buf, headers->value[i]);
        strcat(buf, "\r\n");
    }
    strcat(buf, "\r\n");

    *dst_size = strlen(buf) + request->content_length;

    *dst = malloc(*dst_size);
    if (!(*dst)) {
        free(buf);
        *dst_size = 0;
        return false;
    }

    memcpy(*dst, buf, strlen(buf));
    memcpy(*dst + strlen(buf), request->content, request->content_length);

    free(buf);
    return true;
}

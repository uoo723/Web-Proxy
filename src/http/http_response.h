#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "http_parser.h"
#include "http_request.h"
#include "http_common.h"

typedef struct {
    enum http_status status;
    http_headers_t headers;
    int content_length;
    void *content;
} http_response_t;

/**
 * Get status string from enum http_status defined in http_parser.h
 */
char *get_status_string(enum http_status status);

/**
 * Make response regarding to request.
 */
void make_response(http_response_t *response, http_request_t *request);

/**
 * Make response to raw string for sending via socket.
 *
 * @params dst The result to be stored.
 * @params dst_size The size of dst.
 */
void make_response_string(http_response_t *response, char **dst, int *dst_size);

/**
 * Print http_response_t.
 */
void print_http_response(http_response_t *response);

#ifdef __cplusplus
}
#endif
#endif

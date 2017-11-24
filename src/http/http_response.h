#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include "http_parser.h"
#include "http_common.h"

typedef struct {
    enum http_status status;
    unsigned short http_major;
    unsigned short http_minor;
    http_headers_t headers;
    int content_length;
    bool on_message_completed;
} http_response_t;

/* ********************** Callback for http_parser ********************* */
int response_on_header_field_cb(http_parser *parser, const char *at, size_t len);
int response_on_header_value_cb(http_parser *parser, const char *at, size_t len);
int response_on_body_cb(http_parser *parser, const char *at, size_t len);
int response_on_message_complete_cb(http_parser *parser);
/** ******************************************************************** */

/**
 * Print http_response_t.
 */
void print_http_response(http_response_t *response);

#ifdef __cplusplus
}
#endif
#endif

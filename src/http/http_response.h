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
    http_headers_t *headers;
    int content_length;
    char *content;      // Required manually calling free after use if content_length is not 0
    bool on_message_completed;
} http_response_t;

/**
 * Init http_response_t.
 */
http_response_t *init_http_response(size_t max_num_headers);

/**
 * Free http_response_t.
 */
void free_http_response(http_response_t *response);

/* ********************** Callback for http_parser ********************* */
int response_on_header_field_cb(http_parser *parser, const char *at, size_t len);
int response_on_header_value_cb(http_parser *parser, const char *at, size_t len);
int response_on_body_cb(http_parser *parser, const char *at, size_t len);
int response_on_message_complete_cb(http_parser *parser);
/** ******************************************************************** */

/**
 * Make response string.
 * dst must be free'd after use. But, dst would be NULL. when returned false.
 * dst_size is the size of dst.
 * If making is successful, return true.
 */
bool make_response_string(http_response_t *response, char **dst, size_t *dst_size);

/**
 * Print http_response_t.
 */
void print_http_response(http_response_t *response);

#ifdef __cplusplus
}
#endif
#endif

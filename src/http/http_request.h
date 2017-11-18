#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "http_parser.h"
#include "http_common.h"

#define MAX_HEADERS 50
#define MAX_ELEMENT_SIZE 500

typedef struct {
    char path[MAX_ELEMENT_SIZE];
    enum { REQ_NONE=0, REQ_FIELD, REQ_VALUE } last_header_element;
    http_headers_t headers;
    char body[MAX_ELEMENT_SIZE];
    int method;
    int on_message_completed;
} http_request_t;

/**
 * Print request.
 */
void print_http_request(http_request_t *request);

/* ********************** Callback for http_parser ********************* */
int request_on_url_cb(http_parser *parser, const char *at, size_t len);
int request_on_header_field_cb(http_parser *parser, const char *at, size_t len);
int request_on_header_value_cb(http_parser *parser, const char *at, size_t len);
int request_on_body_cb(http_parser *parser, const char *at, size_t len);
int request_on_message_complete_cb(http_parser *parser);
/** ******************************************************************** */

#ifdef __cplusplus
}
#endif
#endif

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "http_parser.h"
#include "http_common.h"

#define SCHEMA_LEN 16
#define PORT_LEN 8
#define HOST_LEN 128

typedef struct {
    char path[MAX_ELEMENT_SIZE];
    unsigned short http_major;
    unsigned short http_minor;
    http_headers_t headers;
    int content_length;
    char *content;
    int method;
    bool on_message_completed;
    char ip[INET_ADDRSTRLEN];       // For logging
    char schema[SCHEMA_LEN];       // For logging
    char port[PORT_LEN];          // For logging
    char host[HOST_LEN];         // For logging
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

/**
 * Make request string.
 * dst must be free'd after use.
 */
bool make_request_string(http_request_t *request, char **dst, size_t *dst_size);

#ifdef __cplusplus
}
#endif
#endif

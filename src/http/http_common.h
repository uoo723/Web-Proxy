#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_MAX_HEADERS 50
#define MAX_RANGE 50

/**
 * Common struct for http_request_t and http_response_t
 */
typedef struct {
    enum { HEADER_NONE=0, HEADER_FIELD, HEADER_VALUE } last_header_element;
    char **field;
    char **value;
    size_t num_headers;
    size_t max_num_headers;
} http_headers_t;

/**
 * Struct for "Range" header in request.
 */
typedef struct {
    enum { UNIT_NONE=0, BYTES } unit;
    int start[MAX_RANGE];
    int end[MAX_RANGE];
    int num_range;
} range_t;

/**
 * Init http_headers_t.
 * set maximum number of headers. If max_num_headers <= 0,
 * DEFAULT_MAX_HEADERS (50) is applied.
 */
http_headers_t *init_http_headers(size_t max_num_headers);

/**
 * Free http_headers_t.
 */
void free_http_headers(http_headers_t *headers);

/**
 * Set header
 * If header field is already set, it will replace with new value.
 *
 * @params headers Pointer to http_headers_t to be set.
 * @params field String to set field.
 * @params value String to set value associated with field.
 * @return true if set_header is successful, otherwise false.
 */
bool set_header(http_headers_t *headers, const char *field, const char *value);

/**
 * Find header value using search keyword (field).
 *
 * @params headers Pointer to http_headers_t
 * @params search Field keyword.
 * @return If field keyword is found in headers, return the value or NULL otherwise.
 */
char *find_header_value(http_headers_t *headers, const char *search);

/**
 * Get range from "Range" field in header
 *
 * @params str Raw string of value part of "Range" field.
 * @params range The result to be stored.
 * @return If stroing range is succeed, return 0 or -1 otherwise.
 */
int get_range(char *str, range_t *range);

/**
 * Print range struct
 */
void print_range(range_t *range);

#ifdef __cplusplus
}
#endif
#endif

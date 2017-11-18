#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HEADERS 50
#define MAX_ELEMENT_SIZE 500
#define MAX_RANGE 50

/**
 * Common struct for http_request_t and http_response_t
 */
typedef struct {
    enum { HEADER_NONE=0, HEADER_FIELD, HEADER_VALUE } last_header_element;
    char field[MAX_HEADERS][MAX_ELEMENT_SIZE];
    char value[MAX_HEADERS][MAX_ELEMENT_SIZE];
    int num_headers;
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
 * Set header
 * If header field is already set, it will replace with new value.
 *
 * @params headers Pointer to http_headers_t to be set.
 * @params field String to set field.
 * @params value String to set value associated with field.
 */
void set_header(http_headers_t *headers, char *field, char *value);

/**
 * Find header value using search keyword (field).
 *
 * @params headers Pointer to http_headers_t
 * @params search Field keyword.
 * @return If field keyword is found in headers, return the value or NULL otherwise.
 */
char *find_header_value(http_headers_t *headers, char *search);

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

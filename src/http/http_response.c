#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_response.h"

static __thread char *local_content;
static __thread size_t local_content_len;
static __thread size_t local_alloc_content_size;

static char *get_status_string(enum http_status status) {
    switch (status) {
    case HTTP_STATUS_CONTINUE:
        return "100 Continue";

    case HTTP_STATUS_SWITCHING_PROTOCOLS:
        return "101 Switching Protocols";

    case HTTP_STATUS_PROCESSING:
        return "102 Switching Protocols";

    case HTTP_STATUS_OK:
        return "200 OK";

    case HTTP_STATUS_CREATED:
        return "201 Created";

    case HTTP_STATUS_ACCEPTED:
        return "202 Accepted";

    case HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
        return "203 Non-Authoritative Information";

    case HTTP_STATUS_NO_CONTENT:
        return "204 No Content";

    case HTTP_STATUS_RESET_CONTENT:
        return "205 Reset Content";

    case HTTP_STATUS_PARTIAL_CONTENT:
        return "206 Partial Content";

    case HTTP_STATUS_MULTI_STATUS:
        return "207 Multi-Status";

    case HTTP_STATUS_ALREADY_REPORTED:
        return "208 Already Reported";

    case HTTP_STATUS_IM_USED:
        return "226 IM Used";

    case HTTP_STATUS_MULTIPLE_CHOICES:
        return "300 Multiple Choices";

    case HTTP_STATUS_MOVED_PERMANENTLY:
        return "301 Moved Permanently";

    case HTTP_STATUS_FOUND:
        return "302 Found";

    case HTTP_STATUS_SEE_OTHER:
        return "303 See Other";

    case HTTP_STATUS_NOT_MODIFIED:
        return "304 Not Modified";

    case HTTP_STATUS_USE_PROXY:
        return "305 Use Proxy";

    case HTTP_STATUS_TEMPORARY_REDIRECT:
        return "307 Temporary Redirect";

    case HTTP_STATUS_PERMANENT_REDIRECT:
        return "308 Permanent Redirect";

    case HTTP_STATUS_BAD_REQUEST:
        return "400 Bad Request";

    case HTTP_STATUS_UNAUTHORIZED:
        return "401 Unauthorized";

    case HTTP_STATUS_PAYMENT_REQUIRED:
        return "402 Payment Required";

    case HTTP_STATUS_FORBIDDEN:
        return "403 Forbidden";

    case HTTP_STATUS_NOT_FOUND:
        return "404 Not Found";

    case HTTP_STATUS_METHOD_NOT_ALLOWED:
        return "405 Method Not Allowed";

    case HTTP_STATUS_NOT_ACCEPTABLE:
        return "406 Not Acceptable";

    case HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED:
        return "407 Proxy Authentication Required";

    case HTTP_STATUS_REQUEST_TIMEOUT:
        return "408 Request Timeout";

    case HTTP_STATUS_CONFLICT:
        return "409 Conflict";

    case HTTP_STATUS_GONE:
        return "410 Gone";

    case HTTP_STATUS_LENGTH_REQUIRED:
        return "411 Length Required";

    case HTTP_STATUS_PRECONDITION_FAILED:
        return "412 Precondition Failed";

    case HTTP_STATUS_PAYLOAD_TOO_LARGE:
        return "413 Payload Too Large";

    case HTTP_STATUS_URI_TOO_LONG:
        return  "414 URI Too Long";

    case HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE:
        return "415 Unsupported Media Type";

    case HTTP_STATUS_RANGE_NOT_SATISFIABLE:
        return "416 Range Not Satisfiable";

    case HTTP_STATUS_EXPECTATION_FAILED:
        return "417 Expectation Failed";

    case HTTP_STATUS_MISDIRECTED_REQUEST:
        return "421 Misdirected Request";
        break;

    case HTTP_STATUS_UNPROCESSABLE_ENTITY:
        return "422 Unprocessable Entity";

    case HTTP_STATUS_LOCKED:
        return "423 Locked";

    case HTTP_STATUS_FAILED_DEPENDENCY:
        return "424 Failed Dependency";

    case HTTP_STATUS_UPGRADE_REQUIRED:
        return "426 Upgrade Required";

    case HTTP_STATUS_PRECONDITION_REQUIRED:
        return "428 Precondition Required";

    case HTTP_STATUS_TOO_MANY_REQUESTS:
        return "429 Too Many Requests";

    case HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE:
        return "431 Request Header Fields Too Large";

    case HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS:
        return "451 Unavailable For Legal Reasons";

    case HTTP_STATUS_INTERNAL_SERVER_ERROR:
        return "500 Internal Server Error";

    case HTTP_STATUS_NOT_IMPLEMENTED:
        return "501 Not Implemented";

    case HTTP_STATUS_BAD_GATEWAY:
        return "502 Bad Gateway";

    case HTTP_STATUS_SERVICE_UNAVAILABLE:
        return "503 Service Unavailable";

    case HTTP_STATUS_GATEWAY_TIMEOUT:
        return "504 Gateway Timeout";

    case HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED:
        return "505 HTTP Version Not Supported";

    case HTTP_STATUS_VARIANT_ALSO_NEGOTIATES:
        return "506 Variant Also Negotiates";

    case HTTP_STATUS_INSUFFICIENT_STORAGE:
        return "507 Insufficient Storage";

    case HTTP_STATUS_LOOP_DETECTED:
        return "508 Loop Detected";

    case HTTP_STATUS_NOT_EXTENDED:
        return "510 Not Extended";

    case HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED:
        return "511 Network Authentication Required";

    default:
        return NULL;
    }
}

int response_on_header_field_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return -1;

    http_response_t *response = (http_response_t *) parser->data;
    http_headers_t *headers = &response->headers;

    if (headers->last_header_element != HEADER_FIELD) {
        headers->num_headers++;
    }

    strncat(headers->field[headers->num_headers - 1], at, len);
    headers->last_header_element = HEADER_FIELD;

    return 0;
}

int response_on_header_value_cb(http_parser *parser, const char *at, size_t len) {
    if (!parser->data) return -1;

    http_response_t *response = (http_response_t *) parser->data;
    http_headers_t *headers = &response->headers;
    strncat(headers->value[headers->num_headers - 1], at, len);
    headers->last_header_element = HEADER_VALUE;

    return 0;
}

int response_on_body_cb(http_parser *parser, const char *at, size_t len) {
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

int response_on_message_complete_cb(http_parser *parser) {
    if (!parser->data) return -1;

    http_response_t *response = (http_response_t *) parser->data;
    response->on_message_completed = true;
    response->status = parser->status_code;
    response->http_major = parser->http_major;
    response->http_minor = parser->http_minor;

    if (local_content_len != 0) {
        response->content = local_content;
        response->content_length = local_content_len;
        local_content = NULL;
        local_content_len = 0;
        local_alloc_content_size = 0;
    }

    return 0;
}

bool make_response_string(http_response_t *response, char **dst, size_t *dst_size) {
    http_headers_t *headers;
    size_t buf_size;
    char *buf;
    int i;

    headers = &response->headers;
    buf_size = response->content_length + 2048;
    *dst = NULL;
    *dst_size = 0;
    buf = malloc(buf_size);

    if (!buf) {
        return false;
    }

    memset(buf, 0, buf_size);
    *dst_size = 0;

    sprintf(buf, "HTTP/%d.%d %s\r\n", response->http_major,
        response->http_minor, get_status_string(response->status));

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

    *dst_size = strlen(buf) + response->content_length;

    *dst = malloc(*dst_size);
    if (!(*dst)) {
        free(buf);
        *dst_size = 0;
        return false;
    }

    memcpy(*dst, buf, strlen(buf));
    memcpy(*dst + strlen(buf), response->content, response->content_length);

    free(buf);
    return true;
}

void print_http_response(http_response_t *response) {
    http_headers_t *headers = &response->headers;
    int i;
    printf("status: %s\n", get_status_string(response->status));

    for (i = 0; i < headers->num_headers; i++) {
        printf("%s: %s\n", headers->field[i], headers->value[i]);
    }
    printf("\n");
    fflush(stdout);
}

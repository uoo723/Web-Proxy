#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "http_response.h"

static void get_current_time(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm = *gmtime(&now);
    strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

static void set_common_headers(http_response_t *response) {
    char time_string[500] = {0};

    get_current_time(time_string, 500);
    set_header(&response->headers, "Date", time_string);
    set_header(&response->headers, "Connection", "close");
    set_header(&response->headers, "Server", "CServer/0.1");
    set_header(&response->headers, "Accept-Ranges", "bytes");
}

static char *get_mime_type(char *path) {
    char *tmp = strrchr(path, '.');
    char *ext = tmp ? tmp + 1 : NULL;

    if (!ext) {
        return "text/plain";
    }

    if (strcmp(ext, "html") == 0) {
        return "text/html";
    }

    if (strcmp(ext, "css") == 0) {
        return "text/css";
    }

    if (strcmp(ext, "png") == 0) {
        return "image/png";
    }

    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
        return "image/jpeg";
    }

    if (strcmp(ext, "mp4") == 0) {
        return "video/mp4";
    }

    if (strcmp(ext, "pdf") == 0) {
        return "application/pdf";
    }

    return "text/plain";
}

char *get_status_string(enum http_status status) {
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

void make_response(http_response_t *response, http_request_t *request) {
    char path[100] = "html";
    int is_server_error = 0;

    response->status = HTTP_STATUS_OK;
    set_common_headers(response);

    if (request->method != HTTP_GET) {
        response->status = HTTP_STATUS_METHOD_NOT_ALLOWED;
        return;
    }

    if (strcmp(request->path, "/") == 0) {
        strcat(path, "/index.html");
    } else {
        strcat(path, request->path);
    }

    FILE *fp = fopen(path, "rb");   // Open file in binary mode for convenience

    if (!fp) {
        if (errno == ENOENT) {
            strcpy(path, "html/404.html");
            fp = fopen(path, "rb");
            response->status = HTTP_STATUS_NOT_FOUND;

            if (!fp) {
                response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
                is_server_error = 1;
            }
        } else {
            response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
            is_server_error = 1;
        }
    }

    if (!is_server_error) {
        fseek(fp, 0, SEEK_END);
        response->content_length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        response->content = fp;

        char str_content_len[100] = {0};
        sprintf(str_content_len, "%d", response->content_length);
        set_header(&response->headers, "Content-Length", str_content_len);
        set_header(&response->headers, "Content-Type", get_mime_type(path));
    }
}

void make_response_string(http_response_t *response, char **dst, int *dst_size) {
    size_t content_offset = 0;
    char *version = "HTTP/1.1 ";
    char *status_string = get_status_string(response->status);
    http_headers_t *headers = &response->headers;

    int i;
    strcat(*dst, version);
    content_offset += strlen(version);
    strcat(*dst, status_string);
    content_offset += strlen(status_string);
    strcat(*dst, "\r\n");
    content_offset += 2;

    for (i = 0; i < headers->num_headers; i++) {
        strcat(*dst, headers->field[i]);
        content_offset += strlen(headers->field[i]);
        strcat(*dst, ": ");
        content_offset += 2;
        strcat(*dst, headers->value[i]);
        content_offset += strlen(headers->value[i]);
        strcat(*dst, "\r\n");
        content_offset += 2;
    }
    strcat(*dst, "\r\n");
    content_offset += 2;
    *dst_size = content_offset;
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

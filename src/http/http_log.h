#ifndef HTTP_LOG_H
#define HTTP_LOG_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>

#include "http_common.h"
#include "http_request.h"
#include "http_response.h"

/**
 * Set file path to log.
 * If file is not specified, log is printed out to stdout.

 * @params path The file path to log.
 * @return true if succeed, otherwise false. errno will be set.
 */
bool http_log_set_file(const char *path);

/**
 * Log http_request_t.
 *
 * @params request The pointer to a http_requeset_t to log
 * @params response The pointer to a http_response_t to log
 */
void log_http_request(http_request_t *request, http_response_t *response);

#ifdef __cplusplus
}
#endif
#endif

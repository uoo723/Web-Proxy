#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_common.h"

http_headers_t *init_http_headers(size_t max_num_headers) {
    http_headers_t *headers;

    headers = malloc(sizeof(http_headers_t));
    if (!headers) {
        return NULL;
    }

    memset(headers, 0, sizeof(http_headers_t));
    headers->max_num_headers =
        max_num_headers <= 0 ? DEFAULT_MAX_HEADERS : max_num_headers;

    headers->field = malloc(sizeof(char *) * headers->max_num_headers);
    if (!headers->field) {
        free(headers);
        return NULL;
    }
    memset(headers->field, 0, sizeof(char *) * headers->max_num_headers);

    headers->value = malloc(sizeof(char *) * headers->max_num_headers);
    if (!headers->value) {
        free(headers->field);
        free(headers);
        return NULL;
    }
    memset(headers->value, 0, sizeof(char *) * headers->max_num_headers);

    return headers;
}

void free_http_headers(http_headers_t *headers) {
    if (!headers) return;

    int i;
    for (i = 0; i < headers->num_headers; i++) {
        free(headers->field[i]);
        free(headers->value[i]);
    }

    free(headers->field);
    free(headers->value);
    free(headers);
}

bool set_header(http_headers_t *headers, const char *field, const char *value) {
    if (!headers) {
        return false;
    }

    int i;
    char *temp;
    for (i = 0; i < headers->num_headers; i++) {
        if (strcmp(headers->field[i], field) == 0) {
            temp = headers->value[i];
            headers->value[i] = malloc(strlen(value) + 1);
            if (!headers->value[i]) {
                headers->value[i] = temp;
                return false;
            }

            free(temp);
            strcpy(headers->value[i], value);
            return true;
        }
    }

    if (headers->num_headers + 1 > headers->max_num_headers) {
        return false;
    }

    headers->field[headers->num_headers] = malloc(strlen(field) + 1);
    if (!headers->field[headers->num_headers]) {
        return false;
    }

    headers->value[headers->num_headers] = malloc(strlen(value) + 1);
    if (!headers->value[headers->num_headers]) {
        free(headers->field[headers->num_headers]);
        return false;
    }

    strcpy(headers->field[headers->num_headers], field);
    strcpy(headers->value[headers->num_headers], value);
    headers->num_headers++;
    return true;
}

char *find_header_value(http_headers_t *headers, const char *search) {
    int i;
    for (i = 0; i < headers->num_headers; i++) {
        if (strcmp(headers->field[i], search) == 0) {
            return headers->value[i];
        }
    }

    return NULL;
}

int get_range(char *str, range_t *range) {
    memset(range, 0, sizeof(range_t));

    char *tmp_str = malloc(sizeof(strlen(str)) + 1);
    char *tmp2_str = tmp_str;
    if (tmp_str == NULL) {
        return -1;
    }

    strcpy(tmp_str, str);

    char *unit = strsep(&tmp_str, "=");

    if (unit == NULL) {
        free(tmp2_str);
        return -1;
    }

    if (strcmp(unit, "bytes") == 0) {
        range->unit = BYTES;
    } else {
        range->unit = UNIT_NONE;
    }

    char *ranges = strsep(&tmp_str, "=");

    if (ranges == NULL) {
        free(tmp2_str);
        return -1;
    }

    do {
        char *range_str = strsep(&ranges, ",\t");
        if (range_str != NULL) {
            char *start = strsep(&range_str, "-");
            if (start != NULL) {
                range->start[range->num_range] = atoi(start);
                if (strcmp(range_str, "\0") == 0) {
                    range->end[range->num_range++] = -1;
                } else {
                    char *end = strsep(&range_str, "-");
                    if (end != NULL) {
                        range->end[range->num_range++] = atoi(end);
                    }
                }
            }
        }
    } while (ranges != NULL);

    free(tmp2_str);
    return 0;
}

void print_range(range_t *range) {
    char *unit = range->unit == BYTES ? "bytes" : "none";
    printf("%s=", unit);
    int i;
    for (i = 0; i < range->num_range; i++) {
        printf("%d-%d", range->start[i], range->end[i]);
        if (i < range->num_range - 1) {
            printf(", ");
        }
    }
    printf("\n");
    fflush(stdout);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_common.h"

void set_header(http_headers_t *headers, char *field, char *value) {
    int i;
    for (i = 0; i < headers->num_headers; i++) {
        if (strcmp(headers->field[i], field) == 0) {
            memset(headers->value[i], 0, MAX_ELEMENT_SIZE);
            strcpy(headers->value[i], value);
            return;
        }
    }

    strcpy(headers->field[headers->num_headers], field);
    strcpy(headers->value[headers->num_headers], value);
    headers->num_headers++;
}

char *find_header_value(http_headers_t *headers, char *search) {
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

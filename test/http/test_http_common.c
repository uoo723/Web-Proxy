#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>
#include <http/http_common.h>

static int setup(void **state) {
    http_headers_t *headers;
    headers = malloc(sizeof(http_headers_t));
    if (headers == NULL) {
        return -1;
    }

    memset(headers, 0, sizeof(http_headers_t));
    *state = headers;

    return 0;
}

static int teardown(void **state) {
    free(*state);
    return 0;
}

static void test_set_header(void **state) {
    http_headers_t *headers = (http_headers_t *) *state;
    set_header(headers, "field", "value");
    assert_string_equal(headers->field[0], "field");
    assert_string_equal(headers->value[0], "value");
    assert_int_equal(headers->num_headers, 1);

    set_header(headers, "field2", "value2");
    assert_string_equal(headers->field[1], "field2");
    assert_string_equal(headers->value[1], "value2");
    assert_int_equal(headers->num_headers, 2);
}

static void test_set_header_duplicated(void **state) {
    http_headers_t *headers = (http_headers_t *) *state;
    set_header(headers, "field", "value");
    assert_string_equal(headers->field[0], "field");
    assert_string_equal(headers->value[0], "value");
    assert_int_equal(headers->num_headers, 1);

    set_header(headers, "field", "value");
    assert_string_equal(headers->field[0], "field");
    assert_string_equal(headers->value[0], "value");
    assert_int_equal(headers->num_headers, 1);
}

static void test_find_value(void **state) {
    http_headers_t *headers = (http_headers_t *) *state;
    set_header(headers, "field", "value");
    char *value = find_header_value(headers, "field");
    assert_string_equal(value, "value");

    value = find_header_value(headers, "field2");
    assert_null(value);
}

static int test_get_range(void **state) {
    // TODO: Test get_range() function
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_set_header, setup, teardown),
        cmocka_unit_test_setup_teardown(test_set_header_duplicated,
            setup, teardown),
        cmocka_unit_test_setup_teardown(test_find_value, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

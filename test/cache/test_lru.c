#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>

#include <cache/lru.h>

#define CACHE_SIZE (512*1024*1024) 	// 5M
#define OBJECT_SIZE (512*1024)		// 5K

static char *keys[5] = { "zero", "one", "two", "three", "four" };
static char *values[5] = { "value1", "value2", "value3", "value4", "value5" };

static char *long_keys[2] = {
    "www.naver.com/",
    "selab.hanyang.ac.kr/notice.php",
};
static char *long_values[2] = {
    "HTTP/1.1 200 OK\r\n"
    "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
    "Server: Apache/2.2.14 (Win32)\r\n"
    "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
    "Content-Length: 56\r\n"
    "Content-Type: text/html\r\n"
    "Connection: Closed\r\n\r\n"
    "<html>\n"
    "\t<body>\n"
    "\t\t<h1>Hello, World!</h1>\n"
    "\t</body>\n"

    "</html>",
    "HTTP/1.1 200 OK\r\n"
    "Date: Mon, 27 Jul 2009 12:28:53 GMT\r\n"
    "Server: Apache/2.2.14 (Win32)\r\n"
    "Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT\r\n"
    "Content-Length: 58\r\n"
    "Content-Type: text/html\r\n"
    "Connection: Closed\r\n\r\n"
    "<html>\n"
    "\t<body>\n"
    "\t\t<h1>Hello, World!!!</h1>\n"
    "\t</body>\n"
    "</html>"
};
static int setup(void **state) {
    lru_cache_t *cache = lru_cache_init(CACHE_SIZE, OBJECT_SIZE);

    if (!cache) {
        return -1;
    }

    *state = cache;
    return 0;
}

static int teardown(void **state) {
    lru_cache_t *cache = (lru_cache_t *) *state;
    if (lru_cache_free(cache) != LRU_CACHE_NO_ERROR) {
        return -1;
    }

    return 0;
}

static void test_lru_set(void **state) {
    lru_cache_t *cache = (lru_cache_t *) *state;
    lru_cache_error err;
    err = lru_cache_set(cache, keys[0], strlen(keys[0])+1,
        values[0], strlen(values[0])+1);

    assert_true(err == LRU_CACHE_NO_ERROR);
}

static void test_lru_get(void **state) {
    lru_cache_t *cache = (lru_cache_t *) *state;
    char *value;
    size_t value_len;
    lru_cache_error err;
    err = lru_cache_get(cache, keys[0], strlen(keys[0])+1,
        (void **) &value, &value_len);

    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_non_null(value);
    assert_true(value_len == strlen(values[0]) + 1);
    assert_true(strcmp(value, values[0]) == 0);

    err = lru_cache_get(cache, keys[1], strlen(keys[1])+1,
        (void **) &value, &value_len);

    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_null(value);
    assert_true(value_len == 0);
}

static void test_lru_delete(void **state) {
    lru_cache_t *cache = (lru_cache_t *) *state;
    lru_cache_error err;
    char *value;
    size_t value_len;

    err = lru_cache_delete(cache, keys[0], strlen(keys[0])+1);
    assert_true(err == LRU_CACHE_NO_ERROR);

    err = lru_cache_get(cache, keys[0], strlen(keys[0])+1,
        (void **) &value, &value_len);
    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_null(value);
    assert_true(value_len == 0);
}

static void test_hit(void **state) {
    lru_cache_t *cache = (lru_cache_t *) *state;
    lru_cache_error err;
    char *value;
    size_t value_len;

    err = lru_cache_get(cache, long_keys[0], strlen(long_keys[0])+1,
        (void **) &value, &value_len);

    // First no hit.
    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_null(value);
    assert_true(value_len == 0);

    // Store cache.
    err = lru_cache_set(cache, long_keys[0], strlen(long_keys[0])+1,
        long_values[0], strlen(long_values[0])+1);
    assert_true(err == LRU_CACHE_NO_ERROR);

    err = lru_cache_get(cache, long_keys[0], strlen(long_keys[0])+1,
        (void **) &value, &value_len);

    // hit.
    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_non_null(value);
    assert_true(value_len == strlen(long_values[0]) + 1);
    assert_true(memcmp(long_values[0], value, strlen(long_keys[0])+1) == 0);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lru_set),
        cmocka_unit_test(test_lru_get),
        cmocka_unit_test(test_lru_delete),
        cmocka_unit_test(test_hit),
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}

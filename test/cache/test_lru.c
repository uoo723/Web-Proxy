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
    lru_cache_error err;
    err = lru_cache_get(cache, keys[0], strlen(keys[0])+1, (void **) &value);

    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_non_null(value);
    assert_true(strcmp(value, values[0]) == 0);

    err = lru_cache_get(cache, keys[1], strlen(keys[1])+1, (void **) &value);

    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_null(value);
}

static void test_lru_delete(void **state) {
    lru_cache_t *cache = (lru_cache_t *) *state;
    lru_cache_error err;
    err = lru_cache_delete(cache, keys[0], strlen(keys[0])+1);

    assert_true(err == LRU_CACHE_NO_ERROR);

    char *value;

    err = lru_cache_get(cache, keys[0], strlen(keys[0])+1, (void **) &value);

    assert_true(err == LRU_CACHE_NO_ERROR);
    assert_null(value);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lru_set),
        cmocka_unit_test(test_lru_get),
        cmocka_unit_test(test_lru_delete),
    };
    return cmocka_run_group_tests(tests, setup, teardown);
}

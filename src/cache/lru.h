#ifndef LRU_H
#define LRU_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <time.h>
#include <pthread.h>

typedef enum {
	LRU_CACHE_NO_ERROR = 0,
	LRU_CACHE_MISSING_CACHE,
	LRU_CACHE_MISSING_KEY,
	LRU_CACHE_MISSING_VALUE,
	LRU_CACHE_PTHREAD_ERROR,
	LRU_CACHE_NO_MEM,
	LRU_CACHE_VALUE_TOO_LONG
} lru_cache_error;

typedef struct lru_item {
	void *key;
	void *value;
	size_t key_len;
	size_t value_len;
	size_t access_count;
	struct lru_item *next;
} lru_item_t;

typedef struct {
	lru_item_t **items;
	lru_item_t *free_items;
	size_t access_count;
	size_t total_memory;
	size_t free_memory;
	size_t hash_table_size;
	time_t seed;
	pthread_mutex_t *lock;
} lru_cache_t;

/**
 * Initialize lru_cache_t object.
 *
 */
lru_cache_t *lru_cache_init(size_t cache_size, size_t average_len);

/**
 * Free lru_cache_t object.
 *
 */
lru_cache_error lru_cache_free(lru_cache_t *cache);

/**
 * Set item to cache.
 *
 */
lru_cache_error lru_cache_set(lru_cache_t *cache, void *key, size_t key_len,
	void *value, size_t value_len);

/**
 * Get item from cache.
 *
 */
lru_cache_error lru_cache_get(lru_cache_t *cache, void *key, size_t key_len,
	void **value, size_t *value_len);

/**
 * Delete item associated by key from cache.
 *
 */
lru_cache_error lru_cache_delete(lru_cache_t *cache, void *key, size_t key_len);

#ifdef __cplusplus
}
#endif
#endif

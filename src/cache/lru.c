#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lru.h"

#define lru_cache_error(cond, err) if (cond) { return (err); }
#define lru_cache_test_missing_cache(cache) \
	lru_cache_error(!(cache), LRU_CACHE_MISSING_CACHE)
#define lru_cache_test_missing_key(key) \
	lru_cache_error(!(key) || (key) == 0, LRU_CACHE_MISSING_KEY)
#define lru_cache_test_missing_value(value) \
	lru_cache_error(!(value) || (value) == 0, LRU_CACHE_MISSING_VALUE)
#define lru_cache_test_value_too_large(cache, value_len) \
	lru_cache_error(cache->total_memory < value_len, LRU_CACHE_VALUE_TOO_LONG)

#define lock_cache(cache) \
	if (pthread_mutex_lock(cache->lock)) { \
		perror("lru_cache failed to pthread_mutex_lock"); \
		return LRU_CACHE_PTHREAD_ERROR; \
	}

#define unlock_cache(cache) \
	if (pthread_mutex_unlock(cache->lock)) { \
		perror("lru_cache faeild to pthread_mutex_unlock"); \
		return LRU_CACHE_PTHREAD_ERROR; \
	}

/**
 * MurmurHash2
 * http://sites.google.com/site/murmurhash
 */
static uint32_t lru_hash(lru_cache_t *cache, void *key, size_t key_len) {
	uint32_t m = 0x5bd1e995;
	uint32_t r = 24;
	uint32_t h = cache->seed ^ key_len;
	char *data = (char *) key;

	while (key_len >= 4) {
		uint32_t k = *(uint32_t *) data;
		k *= m;
		k ^= k >> r;
		k *= m;
		h *= m;
		h ^= k;
		data += 4;
		key_len -= 4;
	}

	switch (key_len) {
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	}

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h % cache->hash_table_size;
}

/**
 * Compare function
 *
 */
static inline int lru_cache_cmp_keys(lru_item_t *item, void *key, size_t key_len) {
	return item->key_len != key_len ? 1 : memcmp(item->key, key, key_len);
}

/**
 * Remove an item and push it to the free items queue.
 *
 */
static void lru_cache_remove_item(lru_cache_t *cache, lru_item_t *prev,
	lru_item_t *item, uint32_t hash_index) {
	if (prev) {
		prev->next = item->next;
	} else {
		cache->items[hash_index] = item->next;
	}

	cache->free_memory += item->value_len;
	free(item->value);
	free(item->key);

	memset(item, 0, sizeof(lru_item_t));
	item->next = cache->free_items;
	cache->free_items = item;
}

/**
 * Remove the least recently used item.
 *
 */
static void lru_cache_remove_lru(lru_cache_t *cache) {
	lru_item_t *min_item = NULL, *min_prev = NULL;
	lru_item_t *item = NULL, *prev = NULL;
	uint32_t i, min_index = -1;
	size_t min_access_count = -1;

	for (i = 0; i < cache->hash_table_size; i++) {
		item = cache->items[i];
		prev = NULL;

		while (item) {
			if (item->access_count < min_access_count || min_access_count == -1) {
				min_access_count = item->access_count;
				min_item = item;
				min_prev = prev;
				min_index = i;
			}
			prev = item;
			item = item->next;
		}
	}

	if (min_item) {
		lru_cache_remove_item(cache, min_prev, min_item, min_index);
	}
}

/**
 * Pop an existing item of the free queue, or create new item.
 *
 */
static lru_item_t *lru_cache_create_item(lru_cache_t *cache) {
	lru_item_t *item = NULL;

	if (cache->free_items) {
		item = cache->free_items;
		cache->free_items = item->next;
	} else {
		item = malloc(sizeof(lru_item_t));
	}

	return item;
}

lru_cache_t *lru_cache_init(size_t cache_size, size_t average_len) {
	lru_cache_t *cache = malloc(sizeof(lru_cache_t));
	if (!cache) {
		perror("lru_cache cannot create object");
		return NULL;
	}

	memset(cache, 0, sizeof(lru_cache_t));

	cache->hash_table_size = cache_size / average_len;
	cache->total_memory = cache->free_memory = cache_size;
	cache->seed = time(NULL);

	cache->items = malloc(sizeof(lru_cache_t *) * cache->hash_table_size);
	if (!cache->items) {
		perror("lru_cache cannot create hash table");
		free(cache);
		return NULL;
	}

	cache->lock = malloc(sizeof(pthread_mutex_t));
	if (pthread_mutex_init(cache->lock, NULL)) {
		perror("lru_cache cannot create mutex");
		free(cache->items);
		free(cache);
		return NULL;
	}

	return cache;
}

lru_cache_error lru_cache_free(lru_cache_t *cache) {
	lru_cache_test_missing_cache(cache);

	lru_item_t *item = NULL, *next = NULL;
	uint32_t i;

	if (cache->items) {
		for (i = 0; i < cache->hash_table_size; i++) {
			item = cache->items[i];
			while (item) {
				next = item->next;
				free(item);
				item = next;
			}
		}

		free(cache->items);
	}

	if (cache->lock) {
		if (pthread_mutex_destroy(cache->lock)) {
			perror("lru_cache cannot destroy mutex");
			return LRU_CACHE_PTHREAD_ERROR;
		}

		free(cache->lock);
	}

	free(cache);

	return LRU_CACHE_NO_ERROR;
}

lru_cache_error lru_cache_set(lru_cache_t *cache, void *key, size_t key_len,
	void *value, size_t value_len) {
	lru_cache_test_missing_cache(cache);
	lru_cache_test_missing_key(key);
	lru_cache_test_missing_value(value);
	lru_cache_test_value_too_large(cache, value_len);

	lock_cache(cache);

	uint32_t hash_index = lru_hash(cache, key, key_len);
	uint32_t required = 0;
	lru_item_t *item = cache->items[hash_index];
	lru_item_t *prev = NULL;

	void *new_key;
	void *new_value;

	while (item && lru_cache_cmp_keys(item, key, key_len)) {
		prev = item;
		item = item->next;
	}

	if (item) {
		required = value_len - item->value_len;
		free(item->value);
		new_value = malloc(value_len);

		if (!new_value) {
			perror("lru_cache_set cannot create new_value");
			unlock_cache(cache);
			return LRU_CACHE_NO_MEM;
		}

		memcpy(new_value, value, value_len);

		item->value = new_value;
		item->value_len = value_len;
	} else {
		item = lru_cache_create_item(cache);
		if (!item) {
			perror("lru_cache_set cannot create item");
			unlock_cache(cache);
			return LRU_CACHE_NO_MEM;
		}

		memset(item, 0, sizeof(lru_item_t));

		new_key = malloc(key_len);

		if (!new_key) {
			perror("lru_cache_set cannot create new_key");
			unlock_cache(cache);
			return LRU_CACHE_NO_MEM;
		}

		new_value = malloc(value_len);

		if (!new_value) {
			free(new_key);
			perror("lru_cache_set cannot create new_value");
			unlock_cache(cache);
			return LRU_CACHE_NO_MEM;
		}

		memcpy(new_key, key, key_len);
		memcpy(new_value, value, value_len);

		item->key = new_key;
		item->value = new_value;
		item->key_len = key_len;
		item->value_len = value_len;
		required = value_len;

		if (prev) {
			prev->next = item;
		} else {
			cache->items[hash_index] = item;
		}
	}

	item->access_count = ++cache->access_count;

	if (required > 0 && required > cache->free_memory) {
		while (cache->free_memory < required) {
			lru_cache_remove_lru(cache);
		}
	}

	cache->free_memory -= required;

	unlock_cache(cache);

	return LRU_CACHE_NO_ERROR;
}

lru_cache_error lru_cache_get(lru_cache_t *cache, void *key, size_t key_len,
	void **value, size_t *value_len) {
	lru_cache_test_missing_cache(cache);
	lru_cache_test_missing_key(key);

	lock_cache(cache);

	uint32_t hash_index = lru_hash(cache, key, key_len);
	lru_item_t *item = cache->items[hash_index];

	while (item && lru_cache_cmp_keys(item, key, key_len)) {
		item = item->next;
	}

	if (item) {
		*value = item->value;
		*value_len = item->value_len;
		item->access_count++;
	} else {
		*value = NULL;
		*value_len = 0;
	}

	unlock_cache(cache);
	return LRU_CACHE_NO_ERROR;
}

lru_cache_error lru_cache_delete(lru_cache_t *cache, void *key, size_t key_len) {
	lru_cache_test_missing_cache(cache);
	lru_cache_test_missing_key(key);

	lock_cache(cache);

	uint32_t hash_index = lru_hash(cache, key, key_len);
	lru_item_t *item = cache->items[hash_index];
	lru_item_t *prev = NULL;

	while (item && lru_cache_cmp_keys(item, key, key_len)) {
		prev = item;
		item = item->next;
	}

	if (item) {
		lru_cache_remove_item(cache, prev, item, hash_index);
	}

	unlock_cache(cache);
	return LRU_CACHE_NO_ERROR;
}

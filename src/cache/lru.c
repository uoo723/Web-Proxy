#include "lru.h"

lru_queue_t *init_lru_queue(size_t maximum_count) {
	lru_queue_t *queue = malloc(sizeof(lru_queue_t));
	if (queue == NULL) return NULL;

	queue->count = 0;
	queue->front = queue->rear = NULL;
	queue->maximum_count = maximum_count;

	return queue;
}

hash_t *init_hash(size_t capacity) {
	hash_t *hash = malloc(sizeof(hash_t));
	if (hash == NULL) return NULL;

	hash->capacity = capacity;
	hash->array = malloc(sizeof(q_node_t *) * hash->capacity);
	if (hash->array == NULL) {
		free(hash);
		return NULL;
	}

	int i;
	for (i = 0; i < hash->capacity; i++) {
		hash->array[i] = NULL;
	}

	return hash;
}

q_node_t *create_q_node(size_t page_num, void *data) {
	q_node_t *node = malloc(sizeof(q_node_t));
	if (node == NULL) return NULL;

	node->page_num = page_num;
	node->data = data;
	node->prev = node->next = NULL;

	return node;
}

inline bool is_queue_frame_full(lru_queue_t *queue) {
	return queue == NULL ? false : queue->count == queue->maximum_count;
}

inline bool is_queue_empty(lru_queue_t *queue) {
	return queue == NULL ? true : queue->rear == NULL;
}

bool enqueue(lru_queue_t *queue, hash_t *hash, size_t page_num, void *data) {
	if (queue == NULL || hash == NULL) return false;
	if (page_num < 0 || page_num >= hash->capacity) return false;

	if (is_queue_frame_full(queue)) {
		hash->array[queue->rear->page_num] = NULL;
		dequeue(queue);
	}

	q_node_t *node = create_q_node(page_num, data);
	if (node == NULL) return false;
	node->next = queue->front;

	if (is_queue_empty(queue)) {
		queue->rear = queue->front = node;
	} else {
		queue->front->prev = node;
		queue->front = node;
	}

	hash->array[page_num] = node;
	queue->count++;
}

void dequeue(lru_queue_t *queue) {
	if (is_queue_empty(queue)) return;
	if (queue->front == queue->rear) {
		queue->front = NULL;
	}

	q_node_t *node = queue->rear;
	queue->rear = queue->rear->prev;

	if (queue->rear) {
		queue->rear->next = NULL;
	}

	free(node);
	queue->count--;
}

bool reference_page(lru_queue_t *queue, hash_t *hash, size_t page_num, bool *failed) {
	return false;
}

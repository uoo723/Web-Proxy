#ifndef LRU_H
#define LRU_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdbool.h>

/**
 * Comparsion function.
 *
 * If lhs == rhs, must be returned true, otherwise false.
 */
typedef bool (*equal_t)(void *lhs, void *rhs);

typedef struct q_node {
	struct q_node *prev, *next;
	void *data;
} q_node_t;

typedef struct {
	size_t count;
	size_t maximum_count;
	q_node_t *front, *prev;
} lru_queue_t;

typedef struct {
	size_t capacity;
	q_node_t **array;
} hash_t;

/**
 * Initialize lru_queue_t object.
 *
 * @params maximum_count Maximum number of nodes.
 * @return lru_queue_t object if succeed, otherwise NULL.
 */
lru_queue_t *init_lru_queue(size_t maximum_count);

/**
 * Initialize hasn_t object.
 *
 * @params capacity The capcity of hash_t object.
 * @return hash_t object if succeed, otherwise NULL.
 */
hash_t *init_hash(size_t capacity);

/**
 * Create new q_node_t object.
 *
 * @params data The user defined data.
 * @return q_node_t object if succeed, otherwise NULL.
 */
q_node_t *create_q_node(void *data);

/**
 * Check whether queue is full or not.
 *
 * @params queue The lru_queue_t to be checked.
 * @return true if queue is full, otherwise false.
 *		   If queue is NULL pointer return false..
 */
bool is_queue_frame_full(lru_queue_t *queue);

/**
 * Check whether queue is empty or not.
 *
 * @params queue The lru_queue_t to be checked.
 * @return true if queue is empty, otherwise false.
 *		   If queue is NULL pointer return true.
 */
bool is_queue_empty(lru_queue_t *queue);

/**
 * Enqueue operation.
 *
 * @params queue lru_queue_t.
 * @params hash hash_t.
 * @params page_num The page number to be crated to a node.
 * @params data The user defined data.
 * @return true if enqueue succeed, otherwise false(e.g. malloc failed).
 */
bool enqueue(lru_queue_t *queue, hash_t *hash, size_t page_num, void *data);

/**
 * Dequeue operation.
 *
 * @params queue lru_queue_t.
 */
void dequeue(lru_queue_t *queue);

/**
 * If the node with given page_num is in queue, we move the node to front of queue.
 * And if the node is not in queue, we create new node and add to the front of queue.
 *
 * @params queue lru_queue_t.
 * @params hash hash_t.
 * @params page_num size_t.
 * @params failed If not null and operation is failed,
 				  failed will be set to false. (due to the enqueue for example)
 * @return true if hit, otherwise false.
 */
bool reference_page(lru_queue_t *queue, hash_t *hash, size_t page_num, bool *failed);

#ifdef __cplusplus
}
#endif
#endif

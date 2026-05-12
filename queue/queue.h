#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>
#include "node.h"

typedef struct _fifo_queue fifo_queue_t;

/* Returns true if node is currently in a queue. */
bool node_in_queue(node_item_t *node);

/* Creates a bounded FIFO queue with the given capacity.
 * Returns NULL if capacity <= 0 or out of memory. */
fifo_queue_t *queue_create(unsigned capacity);

/* Destroys an empty queue. Asserts that the queue is empty. */
void queue_destroy(fifo_queue_t *queue);

/* Removes and returns the front element, or NULL if empty. */
node_item_t *queue_pop(fifo_queue_t *queue);

/* Returns the front element without removing it, or NULL if empty. */
node_item_t *queue_top(fifo_queue_t *queue);

/* Appends node to the back of the queue.
 * Returns 0 on success, -1 if the queue is at capacity. */
int queue_push(fifo_queue_t *queue, node_item_t *node);

/* Removes and returns the node with the given id, or NULL if not found. */
node_item_t *queue_remove(fifo_queue_t *queue, int id);

/* Returns the number of elements currently in the queue. */
int queue_count(fifo_queue_t *queue);

#endif /* _QUEUE_H_ */

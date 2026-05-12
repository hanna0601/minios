#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>

/* In the threading library, queue nodes ARE thread control blocks. */
typedef struct thread node_item_t;
typedef struct _fifo_queue fifo_queue_t;

bool         node_in_queue(node_item_t *node);
fifo_queue_t *queue_create(unsigned capacity);
void         queue_destroy(fifo_queue_t *queue);
node_item_t *queue_pop(fifo_queue_t *queue);
node_item_t *queue_top(fifo_queue_t *queue);
int          queue_push(fifo_queue_t *queue, node_item_t *node);
node_item_t *queue_remove(fifo_queue_t *queue, int id);
int          queue_count(fifo_queue_t *queue);

/* Priority-ordered insertion (ascending priority value = higher priority). */
int queue_push_sorted(fifo_queue_t *queue, node_item_t *node);

#endif /* _QUEUE_H_ */

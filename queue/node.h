#ifndef _NODE_H_
#define _NODE_H_

#include <stdbool.h>

struct _fifo_queue;

typedef struct _node_item {
    int id;
    struct _node_item *next;
    bool status;   /* true when node is currently in a queue */
} node_item_t;

#endif /* _NODE_H_ */

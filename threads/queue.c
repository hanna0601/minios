#include "queue.h"
#include <stdlib.h>
#include <assert.h>
#include "thread.h"

struct _fifo_queue {
    int capacity;
    int size;
    node_item_t *head;
    node_item_t *tail;
};

bool node_in_queue(node_item_t *node)
{
    return node->status;
}

fifo_queue_t *queue_create(unsigned capacity)
{
    if (capacity == 0) return NULL;
    fifo_queue_t *q = (fifo_queue_t *)malloc(sizeof(fifo_queue_t));
    if (!q) return NULL;
    q->capacity = capacity;
    q->size = 0;
    q->head = NULL;
    q->tail = NULL;
    return q;
}

void queue_destroy(fifo_queue_t *queue)
{
    assert(queue->size == 0);
    free(queue);
}

node_item_t *queue_pop(fifo_queue_t *queue)
{
    if (queue->size == 0) return NULL;
    node_item_t *front = queue->head;
    queue->head = front->next;
    queue->size--;
    if (queue->size == 0) queue->tail = NULL;
    front->status = false;
    front->next = NULL;
    return front;
}

node_item_t *queue_top(fifo_queue_t *queue)
{
    return (queue->size == 0) ? NULL : queue->head;
}

int queue_push(fifo_queue_t *queue, node_item_t *node)
{
    if (queue->size == queue->capacity) return -1;
    assert(!node->status);
    node->next = NULL;
    node->status = true;
    if (queue->size == 0) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
    queue->size++;
    return 0;
}

node_item_t *queue_remove(fifo_queue_t *queue, int id)
{
    if (queue->size == 0) return NULL;

    if (queue->head->id == id) {
        node_item_t *found = queue->head;
        queue->head = found->next;
        if (queue->size == 1) queue->tail = NULL;
        queue->size--;
        found->status = false;
        found->next = NULL;
        return found;
    }

    node_item_t *prev = queue->head;
    node_item_t *curr = prev->next;
    while (curr) {
        if (curr->id == id) {
            prev->next = curr->next;
            if (!prev->next) queue->tail = prev;
            queue->size--;
            curr->status = false;
            curr->next = NULL;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

int queue_count(fifo_queue_t *queue)
{
    return queue->size;
}

/* Insert in ascending-priority order (lower value = higher priority). */
int queue_push_sorted(fifo_queue_t *queue, node_item_t *node)
{
    node->status = true;
    if (queue->size == 0) {
        queue->head = node;
        queue->tail = node;
        queue->size++;
        return 0;
    }

    node_item_t *prev = NULL;
    node_item_t *curr = queue->head;
    while (curr && node->priority >= curr->priority) {
        prev = curr;
        curr = curr->next;
    }

    node->next = curr;
    if (!prev) {
        queue->head = node;
    } else {
        prev->next = node;
    }
    if (!curr) queue->tail = node;
    queue->size++;
    return 0;
}

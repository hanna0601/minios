#include "queue.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void node_init(node_item_t *node)
{
    memset(node, 0, sizeof(node_item_t));
}

static int next_id = 1;

static int create_node_and_push(fifo_queue_t *queue)
{
    node_item_t *item = malloc(sizeof(node_item_t));
    assert(item);
    node_init(item);
    item->id = next_id++;
    printf("pushing item %d\n", item->id);
    int ret = queue_push(queue, item);
    if (ret < 0) free(item);
    return ret;
}

static int pop_and_free_node(fifo_queue_t *queue)
{
    node_item_t *item = queue_pop(queue);
    if (!item) return -1;
    printf("popped  item %d\n", item->id);
    free(item);
    return 0;
}

int main(void)
{
    int ret;

    /* capacity enforcement */
    fifo_queue_t *q = queue_create(1);
    ret = create_node_and_push(q); assert(ret == 0);
    ret = create_node_and_push(q); assert(ret < 0);   /* full */
    ret = pop_and_free_node(q);    assert(ret == 0);
    ret = pop_and_free_node(q);    assert(ret < 0);   /* empty */
    queue_destroy(q);

    /* FIFO ordering */
    int first_id = next_id;
    q = queue_create(4);
    for (int i = 0; i < 4; i++) { ret = create_node_and_push(q); assert(ret == 0); }
    assert(queue_count(q) == 4);
    assert(queue_top(q)->id == first_id);
    for (int i = 0; i < 4; i++) { ret = pop_and_free_node(q); assert(ret == 0); }
    assert(queue_count(q) == 0);
    queue_destroy(q);

    /* remove by id — remove the middle element */
    int mid_id = next_id + 1;   /* id of the second item we're about to push */
    q = queue_create(3);
    for (int i = 0; i < 3; i++) create_node_and_push(q);
    node_item_t *removed = queue_remove(q, mid_id);
    assert(removed && removed->id == mid_id);
    free(removed);
    assert(queue_count(q) == 2);
    while (queue_count(q)) { node_item_t *n = queue_pop(q); free(n); }
    queue_destroy(q);

    printf("all tests passed\n");
    return 0;
}

/*
 * Priority scheduler — threads with lower priority value run first.
 * Preempts the running thread when a higher-priority thread becomes ready.
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

extern int queue_push_sorted(fifo_queue_t *, node_item_t *);

static struct _fifo_queue *prio_ready_queue;

int prio_init(void)
{
    prio_ready_queue = queue_create(THREAD_MAX_THREADS);
    return 0;
}

int prio_enqueue(struct thread *thread)
{
    return queue_push_sorted(prio_ready_queue, thread);
}

struct thread *prio_dequeue(void)
{
    return queue_pop(prio_ready_queue);
}

struct thread *prio_remove(Tid tid)
{
    return queue_remove(prio_ready_queue, tid);
}

void prio_destroy(void)
{
    queue_destroy(prio_ready_queue);
}

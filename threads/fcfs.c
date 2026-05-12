/*
 * First-Come First-Served scheduler.
 * Becomes round-robin once preemption is enabled.
 */

#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

static struct _fifo_queue *ready_queue;

int fcfs_init(void)
{
    ready_queue = queue_create(THREAD_MAX_THREADS);
    return 0;
}

int fcfs_enqueue(struct thread *thread)
{
    return queue_push(ready_queue, thread);
}

struct thread *fcfs_dequeue(void)
{
    return queue_pop(ready_queue);
}

struct thread *fcfs_remove(Tid tid)
{
    return queue_remove(ready_queue, tid);
}

void fcfs_destroy(void)
{
    queue_destroy(ready_queue);
}

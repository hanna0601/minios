/*
 * Random scheduler — selects the next thread uniformly at random.
 */

#include "ut369.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

static struct thread **rq;
static int count = 0;

int rand_init(void)
{
    rq = malloc(sizeof(struct thread *) * THREAD_MAX_THREADS);
    count = 0;
    return rq ? 0 : THREAD_NOMEMORY;
}

int rand_enqueue(struct thread *thread)
{
    assert(!interrupt_enabled());
    if (count >= THREAD_MAX_THREADS) return THREAD_NOMORE;
    rq[count++] = thread;
    return 0;
}

struct thread *rand_dequeue(void)
{
    assert(!interrupt_enabled());
    if (count <= 0) return NULL;
    int i = rand() % count;
    struct thread *ret = rq[i];
    rq[i] = rq[--count];
    return ret;
}

struct thread *rand_remove(Tid tid)
{
    assert(!interrupt_enabled());
    for (int i = 0; i < count; i++) {
        if (rq[i]->id == tid) {
            struct thread *ret = rq[i];
            rq[i] = rq[--count];
            return ret;
        }
    }
    return NULL;
}

void rand_destroy(void)
{
    free(rq);
    rq = NULL;
}

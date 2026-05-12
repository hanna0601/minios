#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "ut369.h"
#include "queue.h"
#include "thread.h"
#include "schedule.h"
#include "interrupt.h"

struct thread *thread_array[THREAD_MAX_THREADS];
struct thread *current_thread;

void thread_init(void)
{
    int e = interrupt_off();

    thread_array[0] = malloc(sizeof(struct thread));
    thread_array[0]->id         = 0;
    thread_array[0]->state      = RUNNING;
    thread_array[0]->exit_code  = 0;
    thread_array[0]->stack_ptr  = NULL;
    thread_array[0]->priority   = 0;
    thread_array[0]->status     = false;
    thread_array[0]->wait_queue = queue_create(THREAD_MAX_THREADS);
    getcontext(&thread_array[0]->context);
    current_thread = thread_array[0];

    interrupt_set(e);
}

Tid thread_id(void)
{
    return current_thread->id;
}

static struct thread *thread_get(Tid tid)
{
    if (tid < 0 || tid >= THREAD_MAX_THREADS) return NULL;
    for (int i = 0; i < THREAD_MAX_THREADS; i++) {
        if (thread_array[i] && thread_array[i]->id == tid)
            return thread_array[i];
    }
    return NULL;
}

static bool thread_runnable(Tid tid)
{
    struct thread *t = thread_get(tid);
    return t && (t->state == RUNNING || t->state == READY);
}

/* Save the current thread's context and restore the next thread's.
 * Uses a stack-allocated flag to distinguish the save path from the resume
 * path — the flag is part of the saved context and will read 1 on resume. */
static void thread_switch(struct thread *next)
{
    if (current_thread == next) return;

    int flag = 0;
    if (getcontext(&current_thread->context) < 0) {
        perror("getcontext"); exit(EXIT_FAILURE);
    }
    if (flag == 1) { flag = 0; return; }   /* resumed here after a later switch */
    if (next->state == EXITED) return;

    if (current_thread->state == RUNNING) current_thread->state = READY;

    if (next->state == KILLED) {
        current_thread = next;
        thread_exit(THREAD_KILLED);
        return;
    }

    current_thread = next;
    current_thread->state = RUNNING;
    flag = 1;
    if (setcontext(&next->context)) { perror("setcontext"); exit(EXIT_FAILURE); }
}

Tid thread_yield(Tid want_tid)
{
    int e = interrupt_off();
    struct thread *next = NULL;

    if (want_tid != THREAD_ANY) {
        if (want_tid == thread_id()) {
            assert(thread_runnable(want_tid));
            interrupt_set(e);
            return want_tid;
        }
        next = scheduler->remove(want_tid);
        if (next) {
            if (thread_runnable(thread_id())) {
                scheduler->enqueue(thread_get(thread_id()));
                thread_switch(next);
            }
        } else {
            want_tid = THREAD_INVALID;
        }
        interrupt_set(e);
        return want_tid;
    }

    next = scheduler->dequeue();
    if (!next) { interrupt_set(e); return THREAD_NONE; }

    /* Priority preemption: if the dequeued thread has lower priority than us,
     * put it back and keep running. */
    if (scheduler->realtime && current_thread->priority < next->priority
            && thread_runnable(thread_id())) {
        scheduler->enqueue(next);
        interrupt_set(e);
        return thread_id();
    }

    if (next->id == thread_id()) {
        assert(thread_runnable(next->id));
        interrupt_set(e);
        return next->id;
    }

    if (thread_runnable(thread_id())) scheduler->enqueue(thread_get(thread_id()));
    thread_switch(next);
    interrupt_set(e);
    return thread_id();
}

static void thread_destroy(struct thread *dead)
{
    if (!dead) return;
    scheduler->remove(dead->id);
    thread_array[dead->id] = NULL;
    if (dead->stack_ptr)  { free(dead->stack_ptr);  dead->stack_ptr  = NULL; }
    if (dead->wait_queue)   queue_destroy(dead->wait_queue);
    free(dead);
}

/* Every new thread begins execution here; this lets thread_create set up a
 * uniform entry point regardless of the function signature. */
static void thread_stub(int (*thread_main)(void *), void *arg)
{
    interrupt_on();
    thread_exit(thread_main(arg));
}

Tid thread_create(int (*fn)(void *), void *parg, int priority)
{
    int e = interrupt_off();

    int idi;
    for (idi = 0; idi < THREAD_MAX_THREADS; idi++)
        if (!thread_array[idi]) break;
    if (idi == THREAD_MAX_THREADS) { interrupt_set(e); return THREAD_NOMORE; }

    void *stack = aligned_alloc(16, THREAD_MIN_STACK + sizeof(struct thread));
    if (!stack) { interrupt_set(e); return THREAD_NOMEMORY; }

    struct thread *t = malloc(sizeof(struct thread));
    t->id         = idi;
    t->status     = false;
    t->next       = NULL;
    t->priority   = priority;
    t->state      = READY;
    t->stack_ptr  = stack;
    t->wait_queue = queue_create(THREAD_MAX_THREADS);
    t->exit_code  = 0;

    /* Build an initial context that will call thread_stub(fn, parg).
     * Stack grows down; RSP is placed 8 bytes below the top so that
     * the ABI 16-byte alignment constraint holds after the implicit
     * call pushes a return address. */
    long rsp = (long)stack + THREAD_MIN_STACK - 8;
    getcontext(&t->context);
    t->context.uc_mcontext.gregs[REG_RIP] = (unsigned long)thread_stub;
    t->context.uc_mcontext.gregs[REG_RDI] = (unsigned long)fn;
    t->context.uc_mcontext.gregs[REG_RSI] = (unsigned long)parg;
    t->context.uc_mcontext.gregs[REG_RSP] = (unsigned long)rsp;
    t->context.uc_stack.ss_sp   = stack;
    t->context.uc_stack.ss_size = THREAD_MIN_STACK + sizeof(struct thread);

    thread_array[idi] = t;
    scheduler->enqueue(t);
    if (scheduler->realtime) {
        int ret = thread_yield(THREAD_ANY);
        assert(ret >= 0);
    }
    interrupt_set(e);
    return idi;
}

Tid thread_kill(Tid tid)
{
    int e = interrupt_off();
    if (tid < 0 || tid >= THREAD_MAX_THREADS || tid == thread_id()) {
        interrupt_set(e); return THREAD_INVALID;
    }
    struct thread *victim = thread_get(tid);
    if (!victim) { interrupt_set(e); return THREAD_INVALID; }
    if (victim->state == KILLED || victim->state == EXITED) {
        interrupt_set(e); return tid;
    }

    if (victim->state == BLOCKED) {
        /* Remove from any wait queue it may be sleeping in. */
        for (int i = 0; i < THREAD_MAX_THREADS; i++) {
            struct thread *t = thread_get(i);
            if (t && t->wait_queue) queue_remove(t->wait_queue, victim->id);
        }
        victim->state     = KILLED;
        victim->exit_code = THREAD_KILLED;
        scheduler->enqueue(victim);
        if (scheduler->realtime) thread_yield(THREAD_ANY);
    }

    victim->state     = KILLED;
    victim->exit_code = THREAD_KILLED;
    interrupt_set(e);
    return tid;
}

void thread_exit(int exit_code)
{
    interrupt_off();
    current_thread->state     = EXITED;
    current_thread->exit_code = exit_code;
    scheduler->remove(thread_id());
    thread_wakeup(current_thread->wait_queue, 1);
    if (thread_yield(THREAD_ANY) == THREAD_NONE)
        ut369_exit(exit_code);
}

void thread_end(void)
{
    for (int i = 0; i < THREAD_MAX_THREADS; i++) {
        if (!thread_array[i]) continue;
        if (thread_array[i]->stack_ptr) {
            free(thread_array[i]->stack_ptr);
            thread_array[i]->stack_ptr = NULL;
        }
        if (thread_array[i]->wait_queue)
            queue_destroy(thread_array[i]->wait_queue);
        free(thread_array[i]);
        thread_array[i] = NULL;
    }
}

/* -------------------------------------------------------------------------
 * Synchronization primitives
 * ------------------------------------------------------------------------- */

Tid thread_wait(Tid tid, int *exit_code)
{
    int e = interrupt_off();
    if (tid < 0 || tid >= THREAD_MAX_THREADS || tid == thread_id()) {
        interrupt_set(e); return THREAD_INVALID;
    }
    struct thread *target = thread_get(tid);
    if (!target) { interrupt_set(e); return THREAD_INVALID; }

    if (target->state == EXITED) {
        if (!target->wait_queue || queue_count(target->wait_queue) == 0) {
            if (exit_code) *exit_code = target->exit_code;
            thread_destroy(target);
            interrupt_set(e);
            return 0;
        }
        interrupt_set(e);
        return THREAD_INVALID;
    }

    thread_sleep(target->wait_queue);

    if (target->state == EXITED && queue_count(target->wait_queue) == 0) {
        if (exit_code) *exit_code = target->exit_code;
        thread_destroy(target);
        interrupt_set(e);
        return 0;
    }

    if (exit_code) *exit_code = target->exit_code;
    interrupt_set(e);
    return 0;
}

Tid thread_sleep(fifo_queue_t *queue)
{
    int e = interrupt_off();
    if (!queue) { interrupt_set(e); return THREAD_INVALID; }

    current_thread->state = BLOCKED;
    queue_push(queue, current_thread);
    Tid next = thread_yield(THREAD_ANY);
    if (next == THREAD_NONE) {
        current_thread->state = RUNNING;
        queue_remove(queue, current_thread->id);
    }
    if (current_thread->state == KILLED)
        thread_exit(current_thread->exit_code);

    interrupt_set(e);
    return next;
}

int thread_wakeup(fifo_queue_t *queue, int all)
{
    int e = interrupt_off();
    if (!queue || queue_count(queue) == 0) { interrupt_set(e); return 0; }

    int woken = 0;
    if (all) {
        while (queue_count(queue) > 0) {
            node_item_t *t = queue_pop(queue);
            if (t->state != KILLED) t->state = READY;
            scheduler->enqueue(t);
            woken++;
        }
    } else {
        node_item_t *t = queue_pop(queue);
        t->state = READY;
        scheduler->enqueue(t);
        woken = 1;
    }
    interrupt_set(e);
    return woken;
}

struct lock {
    int locked;
    fifo_queue_t *wait_queue;
    Tid owner_id;
};

struct lock *lock_create(void)
{
    int e = interrupt_off();
    struct lock *lk = malloc(sizeof(struct lock));
    lk->locked     = 0;
    lk->wait_queue = queue_create(THREAD_MAX_THREADS);
    lk->owner_id   = -1;
    interrupt_set(e);
    return lk;
}

void lock_destroy(struct lock *lock)
{
    assert(lock);
    int e = interrupt_off();
    if (!lock->locked) {
        queue_destroy(lock->wait_queue);
        free(lock);
    }
    interrupt_set(e);
}

void lock_acquire(struct lock *lock)
{
    assert(lock);
    int e = interrupt_off();
    while (lock->locked) thread_sleep(lock->wait_queue);
    lock->locked   = 1;
    lock->owner_id = thread_id();
    interrupt_set(e);
}

void lock_release(struct lock *lock)
{
    assert(lock);
    int e = interrupt_off();
    if (lock->owner_id == thread_id()) {
        lock->locked   = 0;
        lock->owner_id = -1;
        thread_wakeup(lock->wait_queue, 0);
    }
    interrupt_set(e);
}

void set_priority(int priority)
{
    current_thread->priority = priority;
    if (scheduler->realtime) thread_yield(THREAD_ANY);
}

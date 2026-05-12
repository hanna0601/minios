#ifndef _THREAD_H_
#define _THREAD_H_

#include "ut369.h"
#include <stdbool.h>
#include <ucontext.h>

typedef enum {
    RUNNING,
    READY,
    KILLED,
    EXITED,
    BLOCKED
} state_t;

struct thread {
    Tid id;

    /* queue membership (node_item_t fields from queue.h) */
    bool status;
    struct thread *next;

    int priority;
    ucontext_t context;
    state_t state;
    void *stack_ptr;
    fifo_queue_t *wait_queue;
    int exit_code;
};

void thread_init(void);
void thread_end(void);

void ut369_exit(int exit_code);

#endif /* _THREAD_H_ */

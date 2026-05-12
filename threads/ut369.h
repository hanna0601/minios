#ifndef _UT369_H_
#define _UT369_H_

#include <stdbool.h>

#define THREAD_MAX_THREADS 1024
#define THREAD_MIN_STACK   32768

typedef int Tid;

enum {
    THREAD_INVALID  = -1,
    THREAD_ANY      = -2,
    THREAD_NONE     = -3,
    THREAD_NOMORE   = -4,
    THREAD_NOMEMORY = -5,
    THREAD_TODO     = -8,
    THREAD_KILLED   = -9,
};

typedef int (* thread_entry_f)(void *);

struct config {
    const char *sched_name;
    bool preemptive;
    bool verbose;
};

/* Initialize the threading system. Must be called before any thread API. */
void ut369_start(struct config *config);

/* Return the identifier of the currently running thread. */
Tid thread_id(void);

/* Create a new thread running fn(arg) with the given priority.
 * Returns the new thread's Tid, or THREAD_NOMORE / THREAD_NOMEMORY on error. */
Tid thread_create(thread_entry_f fn, void *arg, int priority);

/* Exit the calling thread. Never returns. */
void thread_exit(int exit_code);

/* Send a kill signal to thread tid. Returns tid on success or THREAD_INVALID. */
Tid thread_kill(Tid tid);

/* Yield to thread tid (or THREAD_ANY to let the scheduler choose).
 * Returns the Tid switched to, or THREAD_INVALID / THREAD_NONE on error. */
Tid thread_yield(Tid tid);

/* -----------------------------------------------------------------------
 * Preemptive / synchronization API
 * ----------------------------------------------------------------------- */

#define SIG_INTERVAL 200   /* preemption interval in microseconds */

void interrupt_quiet(void);
int  interrupt_enabled(void);
void spin(int usecs);
int  unintr_printf(const char *fmt, ...);

struct _fifo_queue;
typedef struct _fifo_queue fifo_queue_t;

/* Block the calling thread on queue. Returns switched-to Tid or THREAD_INVALID. */
Tid thread_sleep(fifo_queue_t *queue);

/* Wake one (all == 0) or all (all == 1) threads sleeping on queue.
 * Returns the number of threads woken. */
int thread_wakeup(fifo_queue_t *queue, int all);

/* Wait for thread tid to exit. Copies its exit code if exit_code != NULL.
 * Returns 0 on success or THREAD_INVALID on error. */
int thread_wait(Tid tid, int *exit_code);

/* Set the priority of the calling thread. */
void set_priority(int priority);

struct lock;

struct lock *lock_create(void);
void         lock_destroy(struct lock *lock);
void         lock_acquire(struct lock *lock);
void         lock_release(struct lock *lock);

#endif /* _UT369_H_ */

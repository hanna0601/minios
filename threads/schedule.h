#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <stdbool.h>

struct thread;

#define SCHEDULERS \
    S(rand, false) \
    S(fcfs, false) \
    S(prio, true)

#define S(name, ...) \
    int name ## _init(void); \
    int name ## _enqueue(struct thread *); \
    struct thread * name ## _dequeue(void); \
    struct thread * name ## _remove(Tid tid); \
    void name ## _destroy(void);
    SCHEDULERS
#undef S

struct scheduler {
    const char *name;
    int  (* init)(void);
    int  (* enqueue)(struct thread *);
    struct thread * (* dequeue)(void);
    struct thread * (* remove)(Tid tid);
    void (* destroy)(void);
    bool realtime;
};

extern struct scheduler *scheduler;

bool scheduler_init(const char *);
void scheduler_end(void);

#endif /* _SCHEDULE_H_ */

#include "ut369.h"
#include "thread.h"
#include "interrupt.h"
#include "schedule.h"
#include <string.h>
#include <assert.h>

struct scheduler *scheduler;

struct scheduler schedulers[] = {
#define S(name, type) \
    { #name, name ## _init, name ## _enqueue, name ## _dequeue, \
      name ## _remove, name ## _destroy, type },
    SCHEDULERS
#undef S
};

const int num_schedulers = sizeof(schedulers) / sizeof(struct scheduler);

bool scheduler_init(const char *name)
{
    scheduler = NULL;
    for (int i = 0; i < num_schedulers; i++) {
        if (strcmp(name, schedulers[i].name) == 0) {
            scheduler = &schedulers[i];
            return scheduler->init();
        }
    }
    return false;
}

void scheduler_end(void)
{
    if (scheduler) scheduler->destroy();
    scheduler = NULL;
}

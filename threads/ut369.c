#include "ut369.h"
#include "interrupt.h"
#include "thread.h"
#include "schedule.h"
#include <stdlib.h>
#include <assert.h>

static int exit_status = 0;
static ucontext_t main_context;
static volatile int called = 0;

static void ut369_end(void)
{
    assert(!interrupt_enabled());
    interrupt_end();
    thread_end();
    scheduler_end();
    exit(exit_status);
}

void ut369_start(struct config *config)
{
    srand(0);
    scheduler_init(config->sched_name);
    thread_init();
    if (config->preemptive)
        interrupt_init(config->verbose ? 1 : 0);

    assert(!interrupt_enabled());
    getcontext(&main_context);
    if (called == 1) {
        ut369_end();
        assert(false);
    }
    called = 1;
    interrupt_on();
}

void ut369_exit(int exit_code)
{
    assert(!interrupt_enabled());
    exit_status = exit_code;
    setcontext(&main_context);
    assert(false);
}

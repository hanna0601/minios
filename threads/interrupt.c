#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include "ut369.h"
#include "interrupt.h"

static void interrupt_handler(int sig, siginfo_t *sip, void *contextVP);
static void set_interrupt(void);
static void set_signal(sigset_t *setp);

static int init = 0;
static int loud = 0;

void interrupt_init(int verbose)
{
    struct sigaction action;
    int error;

    assert(!init);
    init = 1;
    loud = verbose;
    action.sa_handler = NULL;
    action.sa_sigaction = interrupt_handler;
    error = sigemptyset(&action.sa_mask);
    assert(!error);
    action.sa_flags = SA_SIGINFO;
    if (sigaction(SIG_TYPE, &action, NULL)) {
        perror("sigaction");
        assert(0);
    }
    interrupt_off();
    set_interrupt();
}

void interrupt_end(void)
{
    signal(SIG_TYPE, SIG_IGN);
    init = 0;
}

int interrupt_on(void)
{
    return interrupt_set(1);
}

int interrupt_off(void)
{
    return interrupt_set(0);
}

int interrupt_set(int enabled)
{
    int ret;
    sigset_t mask, omask;
    set_signal(&mask);
    if (enabled)
        ret = sigprocmask(SIG_UNBLOCK, &mask, &omask);
    else
        ret = sigprocmask(SIG_BLOCK, &mask, &omask);
    assert(!ret);
    return (sigismember(&omask, SIG_TYPE) ? 0 : 1);
}

int interrupt_enabled(void)
{
    sigset_t mask;
    if (!init) return 0;
    int ret = sigprocmask(0, NULL, &mask);
    assert(!ret);
    return (sigismember(&mask, SIG_TYPE) ? 0 : 1);
}

void interrupt_quiet(void)
{
    loud = 0;
}

void spin(int usecs)
{
    struct timeval start, end, diff;
    gettimeofday(&start, NULL);
    while (1) {
        gettimeofday(&end, NULL);
        timersub(&end, &start, &diff);
        if ((diff.tv_sec * 1000000 + diff.tv_usec) >= usecs) break;
    }
}

int unintr_printf(const char *fmt, ...)
{
    int ret, enabled;
    va_list args;
    enabled = interrupt_off();
    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);
    interrupt_set(enabled);
    return ret;
}

static void set_signal(sigset_t *setp)
{
    int ret = sigemptyset(setp);
    assert(!ret);
    ret = sigaddset(setp, SIG_TYPE);
    assert(!ret);
}

static void interrupt_handler(int sig, siginfo_t *sip, void *contextVP)
{
    (void)sig;
    (void)sip;
    (void)contextVP;
    assert(!interrupt_enabled());
    if (loud) {
        static int first = 1;
        static struct timeval start, end, diff;
        gettimeofday(&end, NULL);
        if (!first) timersub(&end, &start, &diff);
        else first = 0;
        start = end;
        printf("interrupt: time diff = %ld us\n", diff.tv_sec * 1000000 + diff.tv_usec);
    }
    set_interrupt();
    thread_yield(THREAD_ANY);
}

static void set_interrupt(void)
{
    struct itimerval val = {
        .it_interval = { 0, 0 },
        .it_value    = { 0, SIG_INTERVAL },
    };
    int ret = setitimer(ITIMER_REAL, &val, NULL);
    assert(!ret);
}

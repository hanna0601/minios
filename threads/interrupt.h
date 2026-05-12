#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <signal.h>

#define SIG_TYPE SIGALRM

void interrupt_init(int verbose);
void interrupt_end(void);

int interrupt_on(void);
int interrupt_off(void);
int interrupt_set(int enabled);

#endif /* _INTERRUPT_H_ */

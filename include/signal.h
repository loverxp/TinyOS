#ifndef SIGNAL_H
#define SIGNAL_H

#include "types.h"

struct task;

#define SIGHUP    1
#define SIGINT    2
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGTERM   15

#define NSIG      16

typedef void (*signal_handler_t)(int);

#define SIG_DFL   ((signal_handler_t)0)
#define SIG_IGN   ((signal_handler_t)1)

#define SIG_ACTION_TERMINATE  0
#define SIG_ACTION_IGNORE     1

void signal_init(void);
int  signal_send(uint32_t pid, int sig);
int  signal_check_and_deliver(struct task* target);
int  signal_get_default_action(int sig);

#endif /* SIGNAL_H */

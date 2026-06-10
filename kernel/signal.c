#include "../include/signal.h"
#include "../include/scheduler.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include "../include/serial.h"

static const uint8_t default_actions[NSIG] = {
    [0]        = SIG_ACTION_IGNORE,
    [SIGHUP]   = SIG_ACTION_TERMINATE,
    [SIGINT]   = SIG_ACTION_TERMINATE,
    [3]        = SIG_ACTION_IGNORE,
    [4]        = SIG_ACTION_IGNORE,
    [5]        = SIG_ACTION_IGNORE,
    [6]        = SIG_ACTION_IGNORE,
    [7]        = SIG_ACTION_IGNORE,
    [8]        = SIG_ACTION_IGNORE,
    [SIGKILL]  = SIG_ACTION_TERMINATE,
    [SIGUSR1]  = SIG_ACTION_TERMINATE,
    [11]       = SIG_ACTION_IGNORE,
    [SIGUSR2]  = SIG_ACTION_TERMINATE,
    [13]       = SIG_ACTION_IGNORE,
    [14]       = SIG_ACTION_IGNORE,
    [SIGTERM]  = SIG_ACTION_TERMINATE,
};

void signal_init(void) {
    printf("[OK] Signal initialized\n");
    serial_writestring("[OK] Signal\n");
}

int signal_get_default_action(int sig) {
    if (sig < 0 || sig >= NSIG) return SIG_ACTION_IGNORE;
    return default_actions[sig];
}

int signal_send(uint32_t pid, int sig) {
    if (sig < 1 || sig >= NSIG) return -1;

    task_t* target = scheduler_find_pid(pid);
    if (!target) return -1;

    target->sig_pending |= (1u << sig);

    if (target->state == TASK_BLOCKED) {
        target->state = TASK_READY;
        target->ipc_wait_obj = NULL;
        target->ipc_wait_type = 0;
        target->sleep_deadline = 0;
    }

    serial_printf("[Signal] Sent signal %d to pid %u\n", sig, pid);
    return 0;
}

int signal_check_and_deliver(task_t* target) {
    if (!target) target = scheduler_get_current();
    if (!target || target->sig_pending == 0) return 0;

    int delivered = 0;

    for (int sig = 1; sig < NSIG; sig++) {
        if (!(target->sig_pending & (1u << sig))) continue;

        target->sig_pending &= ~(1u << sig);
        delivered = 1;

        signal_handler_t handler = target->sig_handlers[sig];

        if (handler == SIG_IGN && sig != SIGKILL) continue;

        if (handler == SIG_DFL) {
            int action = default_actions[sig];
            if (action == SIG_ACTION_TERMINATE) {
                serial_printf("[Signal] Terminating pid %u (signal %d)\n", target->pid, sig);
                target->state = TASK_FINISHED;

                struct task* prev = target;
                while (prev->next != target) prev = prev->next;
                prev->next = target->next;

                need_reschedule = 1;
                return 1;
            }
        }
    }

    return delivered;
}

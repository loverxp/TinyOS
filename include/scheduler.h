#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"

#define TASK_NAME_MAX    16
#define TASK_STACK_SIZE  4096
#define MAX_TASKS        16

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_FINISHED
} task_state_t;

typedef struct task {
    uint32_t pid;
    char name[TASK_NAME_MAX];
    task_state_t state;
    uint32_t esp;
    uint32_t stack_base;
    uint32_t ticks_used;
    uint32_t sleep_deadline;  /* timer tick at which to wake (0 = not sleeping) */
    struct task* next;
} task_t;

typedef void (*task_entry_t)(void);

extern volatile uint8_t need_reschedule;

void scheduler_init(void);
task_t* task_create(const char* name, task_entry_t entry);
task_t* scheduler_pick_next(void);
uint32_t prepare_switch(void);
void task_exit(void);
void task_yield(void);          /* Voluntarily give up CPU */
void task_sleep(uint32_t ms);   /* Sleep for N milliseconds */

#endif // SCHEDULER_H

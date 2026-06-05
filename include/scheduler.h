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
    void*    ipc_wait_obj;    /* pointer to pipe/mqueue waiting on (NULL = not waiting) */
    uint8_t  ipc_wait_type;   /* 0=none, 1=wait-for-read, 2=wait-for-write */
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
task_t* scheduler_get_current(void);
void scheduler_wake_ipc(void* obj, uint8_t wait_type);

#endif // SCHEDULER_H

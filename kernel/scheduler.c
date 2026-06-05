#include "../include/scheduler.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include "../include/io.h"
#include "../include/gdt.h"
#include "../include/timer.h"

static task_t tasks[MAX_TASKS];
static task_entry_t task_entries[MAX_TASKS];
static task_t* current_task = NULL;
static uint32_t next_pid = 0;
volatile uint8_t need_reschedule = 0;

uint32_t hook_esp = 0;
uint32_t new_task_entry = 0;

static uint32_t switch_count = 0;

extern void task_trampoline(void);

void trampoline_debug(uint32_t esp_val, uint32_t entry) {
    static int count = 0;
    if (count++ < 5) {
        serial_printf("[trampoline] ESP=0x%x entry=0x%x\n", esp_val, entry);
    }
}

void switch_frame_dump(uint32_t* frame) {
    static int count = 0;
    if (count++ < 3) {
        serial_printf("[do_switch] new_esp=0x%x frame dump:\n", frame);
        serial_printf("  [0]=0x%x [1]=0x%x [2]=0x%x [3]=0x%x\n",
                      frame[0], frame[1], frame[2], frame[3]);
        serial_printf("  [4]=0x%x [5]=0x%x [6]=0x%x [7]=0x%x\n",
                      frame[4], frame[5], frame[6], frame[7]);
        serial_printf("  [8]=0x%x [9]=0x%x [10]=0x%x [11]=0x%x\n",
                      frame[8], frame[9], frame[10], frame[11]);
        serial_printf("  [12]=0x%x [13]=0x%x\n", frame[12], frame[13]);
    }
}

void scheduler_init(void) {
    memset(tasks, 0, sizeof(tasks));

    task_t* idle = &tasks[0];
    idle->pid = next_pid++;
    strncpy(idle->name, "idle", TASK_NAME_MAX);
    idle->state = TASK_RUNNING;
    idle->stack_base = 0;
    idle->esp = 0;
    idle->ticks_used = 0;
    idle->next = idle;

    current_task = idle;

    printf("[scheduler] Initialized (idle pid=0)\n");
}

task_t* task_create(const char* name, task_entry_t entry) {
    task_t* task = NULL;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FINISHED ||
            (tasks[i].pid == 0 && tasks[i].name[0] == '\0')) {
            task = &tasks[i];
            break;
        }
    }
    if (!task) {
        printf("[scheduler] MAX_TASKS reached\n");
        return NULL;
    }

    uint8_t* stack = (uint8_t*)pmm_alloc_page();
    if (!stack) {
        printf("[scheduler] Out of memory\n");
        return NULL;
    }
    memset(stack, 0, TASK_STACK_SIZE);

    task->pid = next_pid++;
    strncpy(task->name, name, TASK_NAME_MAX);
    task->state = TASK_READY;
    task->stack_base = (uint32_t)stack;
    task->ticks_used = 0;
    task_entries[task->pid] = entry;

    uint32_t* frame = (uint32_t*)(stack + TASK_STACK_SIZE);

    /* Entry point sits above EFLAGS; after iret ESP lands here. */
    *(--frame) = (uint32_t)entry;        /* [56] trampoline reads via [esp] */
    *(--frame) = 0x00000200;             /* [52] EFLAGS (IF=1)              */
    *(--frame) = GDT_KERNEL_CODE;        /* [48] CS  = 0x08                 */
    *(--frame) = (uint32_t)task_trampoline; /* [44] EIP                   */
    *(--frame) = 0;                      /* [40] err_code                   */
    *(--frame) = 0;                      /* [36] int_no                     */
    *(--frame) = 0;                      /* [32] EDI  (pusha)               */
    *(--frame) = 0;                      /* [28] ESI                        */
    *(--frame) = 0;                      /* [24] EBP                        */
    *(--frame) = 0;                      /* [20] ESP_ignored                */
    *(--frame) = 0;                      /* [16] EBX                        */
    *(--frame) = 0;                      /* [12] EDX                        */
    *(--frame) = 0;                      /* [ 8] ECX                        */
    *(--frame) = 0;                      /* [ 4] EAX                        */
    *(--frame) = GDT_KERNEL_DATA;        /* [ 0] DS  (pop eax in stub)     */

    task->esp = (uint32_t)frame;

    uint32_t* v = (uint32_t*)task->esp;
    serial_printf("[sched] task '%s' frame at 0x%x:\n", name, task->esp);
    serial_printf("  DS=0x%x EAX=0x%x ECX=0x%x\n", v[0], v[1], v[3]);
    serial_printf("  EBP=0x%x EDI=0x%x\n", v[6], v[8]);
    serial_printf("  int=0x%x err=0x%x EIP=0x%x CS=0x%x EFLAGS=0x%x\n",
                  v[9], v[10], v[11], v[12], v[13]);
    serial_printf("  entry=0x%x trampoline=0x%x\n",
                  (uint32_t)entry, (uint32_t)task_trampoline);

    task->next = current_task->next;
    current_task->next = task;

    printf("[scheduler] Created '%s' (pid=%u, entry=0x%x, stack=0x%x, esp=0x%x)\n",
           name, task->pid, (uint32_t)entry, (uint32_t)stack, task->esp);
    return task;
}

task_t* scheduler_pick_next(void) {
    if (!current_task) return NULL;

    task_t* next = current_task->next;
    while (next != current_task && next->state != TASK_READY) {
        next = next->next;
    }
    if (next->state != TASK_READY) return NULL;
    return next;
}

uint32_t prepare_switch(void) {
    /* Clean up finished tasks: free stack pages and zero their slot.
     * This runs in IRQ context (cli), so it's safe to walk the array. */
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FINISHED && tasks[i].stack_base != 0) {
            serial_printf("[sched] Freeing stack for '%s' (0x%x)\n",
                          tasks[i].name, tasks[i].stack_base);
            pmm_free_page((void*)tasks[i].stack_base);
            tasks[i].stack_base = 0;
            tasks[i].pid = 0;
            tasks[i].name[0] = '\0';
        }
    }

    /* Wake sleeping tasks whose deadline has passed */
    uint32_t now = timer_get_ticks();
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            tasks[i].sleep_deadline != 0 &&
            now >= tasks[i].sleep_deadline) {
            serial_printf("[sched] Waking '%s' (pid=%u)\n",
                          tasks[i].name, tasks[i].pid);
            tasks[i].state = TASK_READY;
            tasks[i].sleep_deadline = 0;
        }
    }

    task_t* next = scheduler_pick_next();
    if (!next || next == current_task) {
        return 0;
    }

    if (switch_count < 10) {
        uint32_t frame_eip = *(uint32_t*)(next->esp + 44);
        uint32_t frame_cs  = *(uint32_t*)(next->esp + 48);
        serial_printf("[sched] #%u: %s -> %s esp=0x%x EIP=0x%x CS=0x%x\n",
                      switch_count, current_task->name, next->name,
                      next->esp, frame_eip, frame_cs);
    }
    switch_count++;

    current_task->esp = hook_esp;
    current_task->state = TASK_READY;
    next->state = TASK_RUNNING;
    if (next->ticks_used == 0) {
        new_task_entry = (uint32_t)task_entries[next->pid];
    }
    next->ticks_used++;
    current_task = next;

    return next->esp;
}

void task_exit(void) {
    disable_interrupts();
    current_task->state = TASK_FINISHED;

    /* Unlink from the circular ring so the scheduler never sees us again. */
    task_t* prev = current_task;
    while (prev->next != current_task) {
        prev = prev->next;
    }
    if (prev != current_task) {
        prev->next = current_task->next;
    }

    serial_printf("[sched] '%s' (pid=%u) exited\n",
                  current_task->name, current_task->pid);

    need_reschedule = 1;
    enable_interrupts();
    while (1) { halt(); }
}

void task_yield(void) {
    need_reschedule = 1;
}

void task_sleep(uint32_t ms) {
    if (ms == 0) {
        task_yield();
        return;
    }
    /* Timer runs at 50 Hz (1 tick = 20ms) */
    uint32_t ticks = (ms + 19) / 20;  /* round up */
    disable_interrupts();
    current_task->sleep_deadline = timer_get_ticks() + ticks;
    current_task->state = TASK_BLOCKED;
    serial_printf("[sched] '%s' sleeping for %u ticks\n",
                  current_task->name, ticks);
    need_reschedule = 1;
    enable_interrupts();
    /* Halt until next interrupt wakes us */
    while (current_task->state == TASK_BLOCKED) {
        halt();
    }
}

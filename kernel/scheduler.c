#include "../include/scheduler.h"
#include "../include/signal.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/stdio.h"
#include "../include/io.h"
#include "../include/gdt.h"
#include "../include/timer.h"
#include "../include/tss.h"

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
    idle->stdout_pipe = -1;
    idle->stdin_pipe = -1;
    idle->is_forked = 0;
    idle->sig_pending = 0;
    memset(idle->sig_handlers, 0, sizeof(idle->sig_handlers));
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
    task->stdout_pipe = -1;
    task->stdin_pipe = -1;
    task->is_forked = 0;
    task->user_stack_base = 0;
    task->sig_pending = 0;
    memset(task->sig_handlers, 0, sizeof(task->sig_handlers));
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
    while (next != current_task && next->state != TASK_READY && next->state != TASK_RUNNING) {
        next = next->next;
    }
    if (next->state != TASK_READY && next->state != TASK_RUNNING) return NULL;
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
            if (tasks[i].user_stack_base) {
                pmm_free_page((void*)tasks[i].user_stack_base);
                tasks[i].user_stack_base = 0;
            }
            tasks[i].stack_base = 0;
            tasks[i].pid = 0;
            tasks[i].name[0] = '\0';
            tasks[i].ipc_wait_obj = NULL;
            tasks[i].ipc_wait_type = 0;
        }
    }

    /* Wake sleeping tasks whose deadline has passed */
    uint32_t now = timer_get_ticks();
    static uint32_t wake_debug_counter = 0;
    static uint32_t total_wakes = 0;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].sleep_deadline != 0) {
            /* Log every 50 calls to confirm wake logic runs */
            if ((wake_debug_counter++ % 50) == 0) {
                serial_printf("[sched] wake_check: '%s' now=%u deadline=%u delta=%d wakes=%u\n",
                              tasks[i].name, now, tasks[i].sleep_deadline,
                              (int32_t)(now - tasks[i].sleep_deadline),
                              total_wakes);
            }
            if (now >= tasks[i].sleep_deadline) {
                total_wakes++;
                /* Direct serial debug: send 'W' + '\n' via raw outb */
                while ((inb(0x3F8 + 5) & 0x20) == 0);  /* wait LSR THRE */
                outb(0x3F8, 'W');
                while ((inb(0x3F8 + 5) & 0x20) == 0);
                outb(0x3F8, '!');
                while ((inb(0x3F8 + 5) & 0x20) == 0);
                outb(0x3F8, '\n');
                serial_printf("[sched] Waking '%s' (pid=%u) now=%u deadline=%u\n",
                              tasks[i].name, tasks[i].pid,
                              now, tasks[i].sleep_deadline);
                tasks[i].state = TASK_READY;
                tasks[i].sleep_deadline = 0;
                tasks[i].ipc_wait_obj = NULL;
                tasks[i].ipc_wait_type = 0;
            }
        }
    }

    /* Deliver pending signals to all tasks */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].sig_pending && tasks[i].state != TASK_FINISHED) {
            signal_check_and_deliver(&tasks[i]);
        }
    }

    task_t* next = scheduler_pick_next();
    if (!next || next == current_task) {
        return 0;
    }

    if (switch_count < 20) {
        uint32_t frame_eip = *(uint32_t*)(next->esp + 44);
        uint32_t frame_cs  = *(uint32_t*)(next->esp + 48);
        serial_printf("[sched] #%u: %s -> %s esp=0x%x EIP=0x%x CS=0x%x",
                      switch_count, current_task->name, next->name,
                      next->esp, frame_eip, frame_cs);
        if (next->state == TASK_BLOCKED) {
            serial_printf(" *** BUG: next is BLOCKED! deadline=%u ***",
                          next->sleep_deadline);
        }
        /* Dump full frame for forked child tasks */
        if (next->ticks_used == 1 && next->pid > 0) {
            uint32_t* f = (uint32_t*)next->esp;
            serial_printf("\n  frame: DS=0x%x EDI=0x%x ESI=0x%x EBP=0x%x",
                          f[0], f[1], f[2], f[3]);
            serial_printf(" EBX=0x%x EDX=0x%x ECX=0x%x EAX=0x%x",
                          f[5], f[6], f[7], f[8]);
            serial_printf(" int=0x%x err=0x%x EIP=0x%x CS=0x%x EFLAGS=0x%x",
                          f[9], f[10], f[11], f[12], f[13]);
            serial_printf(" uESP=0x%x uSS=0x%x", f[14], f[15]);
        }
        serial_printf("\n");
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

    /* Update TSS.esp0 so the next int 0x80 from this task uses its
     * own kernel stack. For the idle task (stack_base=0) we keep
     * the original TSS kernel stack from kernel.c. */
    if (next->stack_base != 0) {
        tss_set_esp0(next->stack_base + TASK_STACK_SIZE);
    } else {
        /* Reset to dedicated TSS kernel stack */
        extern uint8_t tss_kernel_stack[];
        tss_set_esp0((uint32_t)tss_kernel_stack + 4096);
    }

    return next->esp;
}

/* Emergency stack for task_exit — used so the exiting task's own stack
 * can be safely freed by prepare_switch without corrupting the IRQ
 * handler that is still running on the same stack. */
static uint8_t exit_safe_stack[256] __attribute__((aligned(16)));

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

    /* Switch to a safe emergency stack so the next timer IRQ does NOT
     * use this task's kernel stack. Otherwise prepare_switch() will
     * free this very stack while the IRQ handler runs on it. */
    uint32_t safe_esp = (uint32_t)exit_safe_stack + sizeof(exit_safe_stack) - 16;
    tss_set_esp0(safe_esp);

    need_reschedule = 1;
    enable_interrupts();

    /* Switch ESP to the safe stack and wait for the scheduler to pick
     * a remaining task (e.g. idle). */
    asm volatile(
        "mov %0, %%esp\n"
        "1: hlt\n"
        "jmp 1b\n"
        : : "r"(safe_esp) : "memory"
    );

    /* Never reached */
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
    current_task->ipc_wait_obj = NULL;
    current_task->ipc_wait_type = 0;
    serial_printf("[sched] '%s' sleeping for %u ticks\n",
                  current_task->name, ticks);
    need_reschedule = 1;
    enable_interrupts();
    /* Halt until next interrupt wakes us */
    while (current_task->state == TASK_BLOCKED) {
        halt();
    }
}

task_t* scheduler_get_current(void) {
    return current_task;
}

task_t* scheduler_find_pid(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid && tasks[i].state != TASK_FINISHED)
            return &tasks[i];
    }
    return NULL;
}

void scheduler_wake_ipc(void* obj, uint8_t wait_type) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            tasks[i].ipc_wait_obj == obj &&
            tasks[i].ipc_wait_type == wait_type) {
            tasks[i].state = TASK_READY;
            tasks[i].sleep_deadline = 0;
            tasks[i].ipc_wait_obj = NULL;
            tasks[i].ipc_wait_type = 0;
            break;
        }
    }
}

/* Redirect the current task's stdout to a pipe */
void scheduler_set_stdout_pipe(int pipe_id) {
    if (current_task) {
        current_task->stdout_pipe = pipe_id;
    }
}

/* Redirect the current task's stdin from a pipe */
void scheduler_set_stdin_pipe(int pipe_id) {
    if (current_task) {
        current_task->stdin_pipe = pipe_id;
    }
}

/* ── fork ────────────────────────────────────────────────────────────
 * Clones the current task: allocates a new kernel stack page, copies all
 * stack content (including the syscall register frame), adjusts the child's
 * saved EAX to 0 so that fork() returns 0 in the child.
 *
 * Unlike task_create, this is called from the syscall handler (int 0x80)
 * while the kernel is running on the TSS Ring-0 stack. We identify the TSS
 * stack base by rounding down the regs pointer, allocate a child kernel
 * stack, and copy the TSS stack contents into it.
 *
 * We also clone the user stack (saved in the IRET frame) so parent and
 * child have independent user stacks.
 *
 * Returns child PID (>=0) on success, -1 on failure.
 */
int task_fork(uint32_t* regs) {
    disable_interrupts();

    /* Find a free task slot */
    task_t* child = NULL;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FINISHED ||
            (tasks[i].pid == 0 && tasks[i].name[0] == '\0')) {
            child = &tasks[i];
            break;
        }
    }
    if (!child) {
        enable_interrupts();
        return -1;
    }

    /* ── Kernel Stack ──────────────────────────────────────────────
     * The syscall handler runs on the TSS Ring-0 stack.
     * Round down regs to find the TSS stack page base.                     */
    uint32_t tss_stack_base = (uint32_t)regs & ~0xFFF;
    uint32_t offset = (uint32_t)regs - tss_stack_base;

    if (offset >= TASK_STACK_SIZE) {
        serial_printf("[fork] ERROR: regs offset %u out of bounds\n", offset);
        enable_interrupts();
        return -1;
    }

    /* Allocate child kernel stack page */
    uint8_t* child_kstack = (uint8_t*)pmm_alloc_page();
    if (!child_kstack) {
        enable_interrupts();
        return -1;
    }
    memset(child_kstack, 0, TASK_STACK_SIZE);

    /* Copy the TSS stack content (the syscall frame) into child's stack */
    memcpy(child_kstack, (void*)tss_stack_base, TASK_STACK_SIZE);

    uint32_t child_esp = (uint32_t)child_kstack + offset;

    /* Stack layout as viewed by regs pointer:
     *   [0] = DS, [1] = EDI, [2] = ESI, [3] = EBP, [4] = ESP_saved,
     *   [5] = EBX, [6] = EDX, [7] = ECX, [8] = EAX, [9] = int_no,
     *   [10] = err_code, [11] = EIP, [12] = CS, [13] = EFLAGS,
     *   [14] = user_ESP, [15] = user_SS
     */
    uint32_t* child_frame = (uint32_t*)child_esp;

    /* Set child's saved EAX to 0 so fork() returns 0 in child */
    child_frame[8] = 0;

    /* ── User Stack ────────────────────────────────────────────────
     * Clone the user stack so parent and child have independent stacks.  */
    uint32_t parent_user_esp = regs[14];
    uint32_t parent_ustack_base = parent_user_esp & ~0xFFF;

    uint8_t* child_ustack = (uint8_t*)pmm_alloc_page();
    if (!child_ustack) {
        pmm_free_page(child_kstack);
        enable_interrupts();
        return -1;
    }
    memcpy(child_ustack, (void*)parent_ustack_base, TASK_STACK_SIZE);

    uint32_t child_user_esp = (uint32_t)child_ustack +
                              (parent_user_esp - parent_ustack_base);
    child_frame[14] = child_user_esp;   /* update child's saved user ESP */

    /* ── Fill in child TCB ────────────────────────────────────────── */
    child->pid = next_pid++;
    sprintf(child->name, "fork_%u", child->pid);
    child->state = TASK_READY;
    child->stack_base = (uint32_t)child_kstack;
    child->user_stack_base = (uint32_t)child_ustack;
    child->esp = child_esp;
    child->ticks_used = 1;        /* non-zero → skip trampoline path */
    child->sleep_deadline = 0;
    child->ipc_wait_obj = NULL;
    child->ipc_wait_type = 0;
    child->stdout_pipe = -1;
    child->stdin_pipe = -1;
    child->is_forked = 1;
    child->sig_pending = 0;
    memset(child->sig_handlers, 0, sizeof(child->sig_handlers));

    /* Insert into circular linked list */
    child->next = current_task->next;
    current_task->next = child;

    serial_printf("[fork] child pid=%u kstack=0x%x esp=0x%x "
                  "ustack=0x%x uesp=0x%x offset=%u\n",
                  child->pid, (uint32_t)child_kstack, child_esp,
                  (uint32_t)child_ustack, child_user_esp, offset);

    enable_interrupts();
    return child->pid;
}

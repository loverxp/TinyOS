; switch.asm - Context switch assembly routines

extern new_task_entry

; void do_switch(uint32_t new_esp)
; Switches ESP to new_esp and resumes the new task.
; For a previously-running task, ESP was saved by irq_common_stub, so we
; return there and let it do popa/iret.
; For a BRAND-NEW task (ticks_used was 0 before switch), the fake frame has
; an iret structure, so we do pop eax (DS), popa, add esp 8, iret inline.
;
; In this implementation we always go through the inline iret path. The
; irq_common_stub saves ESP *after* pusha/DS-push. The stack layout there
; (low to high) is: DS, EDI..EAX (pusha), int_no, err_code, EIP, CS, EFLAGS.
; This matches the inline restore below.
global do_switch
do_switch:
    add esp, 4            ; Discard do_switch's own return address
    mov esp, [esp]        ; Load new task's ESP (now at [esp+0] after the add)

    ; Inline iret sequence (same as irq_common_stub's restore):
    pop eax               ; DS saved by push eax in stub (or fake frame)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa                  ; Restore pusha registers
    add esp, 8            ; Skip int_no + err_code
    iret                  ; EIP, CS, EFLAGS (for new task: jumps to trampoline)

; void task_trampoline(void)
; Entered via iret. After iret, ESP points at the entry dword above EFLAGS.
; Reads entry from the global new_task_entry (set by prepare_switch) and jumps.
global task_trampoline
task_trampoline:
    jmp [new_task_entry]

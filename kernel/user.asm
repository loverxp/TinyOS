; user.asm - User mode tasks and Ring 3 switch helpers

section .bss
user_stack:
    resb 4096
user_stack_top:

section .data
global saved_kernel_esp
saved_kernel_esp dd 0

section .text

; ------------------------------------------------------------------
; run_user_task(target_func) - Switch to Ring 3, run user code,
;                              return to kernel when user calls syscall 0
; ------------------------------------------------------------------
global run_user_task
run_user_task:
    mov eax, [esp + 4]      ; target user function address

    ; Save kernel stack pointer so user_exit_handler can return to caller
    mov [saved_kernel_esp], esp

    ; Set up user data segments (0x23 = GDT[4] | RPL=3)
    mov ebx, 0x23
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    ; Build IRET frame for Ring 3
    ;   0x1B = GDT[3] | RPL=3 (user code)
    ;   0x23 = GDT[4] | RPL=3 (user data)
    push 0x23               ; SS  = user data with RPL=3
    push user_stack_top     ; ESP = user stack top
    pushfd                  ; EFLAGS
    or  dword [esp], 0x3200 ; Set IF=1 (enable interrupts) and IOPL=3
    push 0x1B               ; CS  = user code with RPL=3
    push eax                ; EIP = user function
    iret

; ------------------------------------------------------------------
; user_exit_handler - Called by syscall 0 to return from Ring 3
;                     to kernel mode. Restores kernel stack and returns
;                     to the caller of run_user_task.
; ------------------------------------------------------------------
global user_exit_handler
user_exit_handler:
    ; Restore kernel data segments
    mov ax, 0x10            ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore kernel stack pointer and return to caller of run_user_task
    mov esp, [saved_kernel_esp]
    ret

; ------------------------------------------------------------------
; user_main - User test program, runs in Ring 3
; ------------------------------------------------------------------
global user_main
user_main:
    ; Test: print message via syscall 1
    mov eax, 1
    int 0x80

    ; Test: execute privileged instruction hlt (should trigger GPF)
    ; This will be caught, reported, and skipped by exception handler
    hlt

    ; Exit back to kernel via syscall 0
    mov eax, 0
    int 0x80

    ; Should never reach here
.loop:
    jmp .loop
; interrupts.asm - Interrupt Service Routines

extern isr_handler
extern irq_handler
extern prepare_switch
extern do_switch
extern need_reschedule
extern hook_esp

; ISR macro - creates a stub for ISRs without error code
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push byte 0     ; Dummy error code
    push byte %1    ; Interrupt number
    jmp isr_common_stub
%endmacro

; ISR macro - creates a stub for ISRs with error code
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push byte %1    ; Interrupt number
    jmp isr_common_stub
%endmacro

; IRQ macro - creates a stub for IRQs
%macro IRQ 2
global irq%1
irq%1:
    cli
    push byte 0     ; Dummy error code
    push byte %2    ; Interrupt number (remapped)
    jmp irq_common_stub
%endmacro

; Define all ISRs
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; Define all IRQs (remapped to 32-47)
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; Syscall interrupt (int 0x80) - can be called from Ring 3
extern syscall_handler
global isr128
isr128:
    cli
    push byte 0     ; Dummy error code
    push dword 128   ; Interrupt number (0x80)
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10    ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call C handler with pointer to register frame
    ; regs[0]=ds, [1]=edi, ..., [8]=eax, [9]=int_no, [10]=err_code
    mov eax, esp
    push eax
    call syscall_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

; Common ISR stub
isr_common_stub:
    pusha           ; Push all registers (32 bytes)
    
    mov ax, ds
    push eax        ; Save data segment (4 bytes)
    
    mov ax, 0x10    ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Get int_no and err_code from stack
    ; Stack: [esp]=ds, [esp+4]=edi(pa)...[esp+36]=int_no,[esp+40]=err_code
    mov ebx, [esp + 36]   ; int_no
    mov ecx, [esp + 40]   ; err_code

    ; Push regs frame pointer (esp points to saved ds)
    mov eax, esp
    push eax              ; Push regs pointer
    push ecx              ; Push err_code
    push ebx              ; Push int_no
    call isr_handler
    add esp, 12           ; Clean up all 3 parameters
    
    pop eax         ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa            ; Pop all registers
    add esp, 8      ; Clean up error code and interrupt number
    iret

; Common IRQ stub
irq_common_stub:
    pusha           ; Push all registers
    
    mov ax, ds
    push eax        ; Save data segment
    
    mov ax, 0x10    ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Get int_no from stack, convert to IRQ number
    mov ebx, [esp + 36]   ; Get int_no (remapped vector: 32-47)
    sub ebx, 32           ; Convert to IRQ number (0-15)

    ; Push regs frame pointer
    mov eax, esp
    push eax              ; Push regs pointer
    push ebx              ; Push IRQ number
    call irq_handler
    add esp, 8            ; Clean up 2 parameters

    ; ── Scheduler hook ──
    cmp byte [need_reschedule], 0
    je .no_schedule
    mov byte [need_reschedule], 0
    mov [hook_esp], esp    ; Save ESP at this point (pusha frame top)
    call prepare_switch    ; Returns new task's ESP in EAX (0 = no switch)
    test eax, eax
    jz .no_schedule
    push eax               ; Push new_esp as argument for do_switch
    call do_switch          ; Switches ESP, returns on new task's stack
    add esp, 4              ; Clean up do_switch argument
.no_schedule:
    ; ── End scheduler hook ──

    ; ── Debug: dump pre-iret state to serial ──
    push eax
    push edx
    mov edx, 0x3F8
.dbg_w1:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_w1
    mov al, 0x3C        ; '<'
    out dx, al
.dbg_w2:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_w2
    mov al, 0x49        ; 'I'
    out dx, al
.dbg_w3:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_w3
    mov al, 0x3E        ; '>'
    out dx, al
    pop edx
    pop eax
    ; ── End debug ──

    pop eax         ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ── Debug: after segment restore ──
    push eax
    push edx
    mov edx, 0x3F8
.dbg_s1:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_s1
    mov al, 0x53        ; 'S'
    out dx, al
    pop edx
    pop eax
    ; ── End debug ──

    popa            ; Pop all registers

    ; ── Debug: after popa ──
    push eax
    push edx
    mov edx, 0x3F8
.dbg_p1:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_p1
    mov al, 0x50        ; 'P'
    out dx, al
    pop edx
    pop eax
    ; ── End debug ──

    add esp, 8      ; Clean up error code and interrupt number

    ; ── Debug: dump EIP from iret frame (at [ESP+0] now) and print 'R' ──
    push eax
    push edx
    push ecx
    mov ecx, [esp + 12]  ; EIP (above the 3 saved regs: eax/edx/ecx = 12 bytes)
    mov edx, 0x3F8
    ; Print 8 hex digits of EIP
    mov al, 0x45         ; 'E'
.dbg_reip_w:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_reip_w
    mov al, 0x45
    out dx, al
.dbg_reip_i:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_reip_i
    mov al, 0x49         ; 'I'
    out dx, al
.dbg_reip_p:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_reip_p
    mov al, 0x50         ; 'P'
    out dx, al
.dbg_reip_eq:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_reip_eq
    mov al, 0x3D         ; '='
    out dx, al
    ; Now print EIP in hex
    mov eax, ecx
    mov cl, 28
.dbg_reip_loop:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_reip_loop
    mov eax, ecx
    shr eax, cl
    and al, 0x0F
    cmp al, 10
    jl .dbg_reip_digit
    add al, 0x37         ; 'A' - 10
    jmp .dbg_reip_out
.dbg_reip_digit:
    add al, 0x30         ; '0'
.dbg_reip_out:
    out dx, al
    sub cl, 4
    jnc .dbg_reip_loop
    ; Print newline
.dbg_reip_nl1:
    in al, 0x3FD
    test al, 0x20
    jz .dbg_reip_nl1
    mov al, 0x0A
    out dx, al
    pop ecx
    pop edx
    pop eax
    ; ── End debug ──

    iret

; IDT load function
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

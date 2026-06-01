; interrupts.asm - Interrupt Service Routines
; This file contains the assembly stubs for all ISRs and IRQs

; External C handlers
extern isr_handler
extern irq_handler

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
; %1 = irq number (0-15), %2 = remapped vector (32-47)
%macro IRQ 2
global irq%1
irq%1:
    cli
    push byte 0     ; Dummy error code
    push byte %2    ; Interrupt number (remapped vector)
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

; Common ISR stub
; Stack layout when entering:
;   [esp+12] int_no
;   [esp+8]  err_code
;   [esp+4]  eax (saved ds)
;   [esp]    pusha regs
isr_common_stub:
    pusha           ; Push all registers
    
    mov ax, ds
    push eax        ; Save data segment
    
    mov ax, 0x10    ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Call C handler with parameters (int_no, err_code)
    ; Parameters are already on stack from ISR stub
    push esp        ; Pass pointer to register frame (optional)
    call isr_handler
    add esp, 4      ; Clean up pushed esp
    
    pop eax         ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa            ; Pop all registers
    add esp, 8      ; Clean up error code and interrupt number
    sti
    iret            ; Return from interrupt

; Common IRQ stub
; Stack layout when entering:
;   [esp+12] int_no (remapped vector 32-47)
;   [esp+8]  dummy error code (0)
;   [esp+4]  eax (saved ds)
;   [esp]    pusha regs
irq_common_stub:
    pusha           ; Push all registers
    
    mov ax, ds
    push eax        ; Save data segment
    
    mov ax, 0x10    ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Calculate IRQ number from interrupt vector
    ; int_no is at [esp+44] after pusha (8 regs * 4 bytes) + saved eax
    ; Actually: pusha = 32 bytes, saved eax = 4 bytes
    ; So int_no is at [esp+36] relative to current esp
    ; But we need to access it from the original stack frame
    
    ; Easier: the int_no was pushed before pusha
    ; After pusha (32 bytes) + push eax (4 bytes):
    ; [esp+36] = int_no, [esp+40] = err_code
    
    mov ebx, [esp + 36]   ; Get int_no (remapped vector)
    sub ebx, 32           ; Convert to IRQ number (0-15)
    push ebx              ; Pass IRQ number as parameter
    call irq_handler
    add esp, 4            ; Clean up parameter
    
    pop eax         ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa            ; Pop all registers
    add esp, 8      ; Clean up error code and interrupt number
    sti
    iret            ; Return from interrupt

; IDT load function
global idt_load
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; gdt.asm - Global Descriptor Table setup
; This file sets up the GDT and performs the far jump to protected mode

section .data
align 4
; GDT structure
gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0

    ; Code segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Access byte: present, ring 0, code, executable, direction 0, readable
    db 11001111b    ; Flags + Limit (bits 16-19): 4KB granularity, 32-bit, limit high nibble
    db 0x0          ; Base (bits 24-31)

    ; Data segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10010010b    ; Access byte: present, ring 0, data, direction 0, writable
    db 11001111b    ; Flags + Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size of GDT
    dd gdt_start                ; Base of GDT

section .text
; Segment selectors
CODE_SEG equ 0x08
DATA_SEG equ 0x10

; GDT initialization function
global gdt_init
gdt_init:
    lgdt [gdt_descriptor]
    
    ; Reload segment registers
    jmp CODE_SEG:.reload_cs
    
.reload_cs:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

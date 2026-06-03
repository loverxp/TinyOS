; gdt.asm - Legacy GDT data (no longer used for initialization)
; GDT is now managed from kernel/gdt.c for dynamic TSS support.
; Kept for reference.

section .data
align 4
gdt_start:
    dd 0x0
    dd 0x0
    dw 0xFFFF, 0x0, 0x0, 10011010b, 11001111b, 0x0
    dw 0xFFFF, 0x0, 0x0, 10010010b, 11001111b, 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
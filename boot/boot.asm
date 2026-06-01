; boot.asm - Multiboot compliant boot loader
; This file sets up the stack and calls the kernel main function

MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
resb 16384 ; 16 KiB stack
stack_top:

section .text
global start:function (start.end - start)

start:
    ; The bootloader has loaded us into 32-bit protected mode on a x86 machine.
    ; Interrupts are disabled. Paging is disabled.

    ; Set up the stack.
    mov esp, stack_top

    ; Call the kernel main function.
    extern kernel_main
    call kernel_main

    ; If kernel_main returns (it shouldn't), hang.
    cli
.hang:		hlt
    jmp .hang
.end:

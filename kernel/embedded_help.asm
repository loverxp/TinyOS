; embedded_help.asm - Embed help user program ELF binary into kernel

global embedded_help_start, embedded_help_end

section .rodata
embedded_help_start:
    incbin "build/user/help.elf"
embedded_help_end:
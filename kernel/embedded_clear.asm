; embedded_clear.asm - Embed clear user program ELF binary into kernel

global embedded_clear_start, embedded_clear_end

section .rodata
embedded_clear_start:
    incbin "build/user/clear.elf"
embedded_clear_end:
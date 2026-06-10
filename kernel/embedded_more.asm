; embedded_more.asm - Embed more user program ELF binary into kernel

global embedded_more_start, embedded_more_end

section .rodata
embedded_more_start:
    incbin "build/user/more.elf"
embedded_more_end:
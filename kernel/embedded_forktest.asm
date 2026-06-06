; embedded_forktest.asm - Embed forktest user program ELF binary into kernel

global embedded_forktest_start, embedded_forktest_end

section .rodata
embedded_forktest_start:
    incbin "build/user/forktest.elf"
embedded_forktest_end:
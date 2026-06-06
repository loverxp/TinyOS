; embedded_rand.asm - Embed rand user program ELF binary into kernel

global embedded_rand_start, embedded_rand_end

section .rodata
embedded_rand_start:
    incbin "build/user/rand.elf"
embedded_rand_end:
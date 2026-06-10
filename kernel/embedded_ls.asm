; embedded_ls.asm - Embed ls user program ELF binary into kernel

global embedded_ls_start, embedded_ls_end

section .rodata
embedded_ls_start:
    incbin "build/user/ls.elf"
embedded_ls_end:

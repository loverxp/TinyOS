; embedded_cat.asm - Embed cat user program ELF binary into kernel

global embedded_cat_start, embedded_cat_end

section .rodata
embedded_cat_start:
    incbin "build/user/cat.elf"
embedded_cat_end:

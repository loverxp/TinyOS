; embedded_date.asm - Embed date user program ELF binary into kernel

global embedded_date_start, embedded_date_end

section .rodata
embedded_date_start:
    incbin "build/user/date.elf"
embedded_date_end:
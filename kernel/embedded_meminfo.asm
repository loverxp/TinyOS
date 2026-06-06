; embedded_meminfo.asm - Embed meminfo user program ELF binary into kernel

global embedded_meminfo_start, embedded_meminfo_end

section .rodata
embedded_meminfo_start:
    incbin "build/user/meminfo.elf"
embedded_meminfo_end:
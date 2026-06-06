; embedded_diskinfo.asm - Embed diskinfo user program ELF binary into kernel

global embedded_diskinfo_start, embedded_diskinfo_end

section .rodata
embedded_diskinfo_start:
    incbin "build/user/diskinfo.elf"
embedded_diskinfo_end:
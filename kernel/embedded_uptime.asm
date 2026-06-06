; embedded_uptime.asm - Embed uptime user program ELF binary into kernel

global embedded_uptime_start, embedded_uptime_end

section .rodata
embedded_uptime_start:
    incbin "build/user/uptime.elf"
embedded_uptime_end:
; embedded_ping.asm
global embedded_ping_start, embedded_ping_end
section .rodata
embedded_ping_start:
    incbin "build/user/ping.elf"
embedded_ping_end:

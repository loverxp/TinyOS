; embedded_kill.asm
global embedded_kill_start, embedded_kill_end
section .rodata
embedded_kill_start:
    incbin "build/user/kill.elf"
embedded_kill_end:

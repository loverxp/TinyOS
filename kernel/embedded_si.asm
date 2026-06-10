; embedded_si.asm
global embedded_si_start, embedded_si_end
section .rodata
embedded_si_start:
    incbin "build/user/si.elf"
embedded_si_end:

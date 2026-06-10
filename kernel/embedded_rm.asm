; embedded_rm.asm
global embedded_rm_start, embedded_rm_end
section .rodata
embedded_rm_start:
    incbin "build/user/rm.elf"
embedded_rm_end:

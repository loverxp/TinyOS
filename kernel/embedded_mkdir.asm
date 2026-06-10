; embedded_mkdir.asm
global embedded_mkdir_start, embedded_mkdir_end
section .rodata
embedded_mkdir_start:
    incbin "build/user/mkdir.elf"
embedded_mkdir_end:

; embedded_rmdir.asm
global embedded_rmdir_start, embedded_rmdir_end
section .rodata
embedded_rmdir_start:
    incbin "build/user/rmdir.elf"
embedded_rmdir_end:

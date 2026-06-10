; embedded_write.asm
global embedded_write_start, embedded_write_end
section .rodata
embedded_write_start:
    incbin "build/user/write.elf"
embedded_write_end:

; embedded_dino.asm
global embedded_dino_start, embedded_dino_end
section .rodata
embedded_dino_start:
    incbin "build/user/dino.elf"
embedded_dino_end:
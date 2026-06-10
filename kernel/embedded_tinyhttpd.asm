; embedded_tinyhttpd.asm
global embedded_tinyhttpd_start, embedded_tinyhttpd_end
section .rodata
embedded_tinyhttpd_start:
    incbin "build/user/tinyhttpd.elf"
embedded_tinyhttpd_end:

; embedded_hello.asm - Embed hello user program ELF binary into kernel

global embedded_hello_start, embedded_hello_end

section .rodata
embedded_hello_start:
    incbin "build/user/hello.elf"
embedded_hello_end:
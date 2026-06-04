; embedded_hello.asm - Embed hello user program binary into kernel

global embedded_hello_start, embedded_hello_end

section .rodata
embedded_hello_start:
    incbin "build/user/hello.bin"
embedded_hello_end:
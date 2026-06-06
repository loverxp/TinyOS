; embedded_echo.asm - Embed echo user program ELF binary into kernel

global embedded_echo_start, embedded_echo_end

section .rodata
embedded_echo_start:
    incbin "build/user/echo.elf"
embedded_echo_end:
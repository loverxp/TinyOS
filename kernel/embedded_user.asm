; embedded_user.asm - Embed user program binary into kernel
; This file is auto-generated - the binary must exist before kernel build

global embedded_user_start, embedded_user_end

section .rodata
embedded_user_start:
    incbin "build/user/gfxsnake.bin"
embedded_user_end:
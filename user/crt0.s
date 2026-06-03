; crt0.s - User program startup code
; Called by kernel when program is loaded at 0x400000
; Sets up C runtime and calls main(), then exits via syscall

extern main

section .text
global _start
_start:
    ; Clear BSS
    extern __bss_start, __bss_end
    mov ecx, __bss_end
    sub ecx, __bss_start
    je .bss_done
    mov edi, __bss_start
    xor eax, eax
    cld
    rep stosb
.bss_done:

    call main

    ; Exit via syscall 0
    push eax        ; exit code
    call __libc_exit
    ; Should never reach here
    hlt

; __libc_exit(int code) - exit via syscall 0
global __libc_exit
__libc_exit:
    mov eax, 0      ; syscall 0: return to kernel
    int 0x80
    ret
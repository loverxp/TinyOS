; io.asm - I/O port operations
; These are assembly implementations of the port I/O functions

section .text

; void outb(uint16_t port, uint8_t value)
global outb
outb:
    mov al, [esp + 8]   ; value
    mov dx, [esp + 4]   ; port
    out dx, al
    ret

; void outw(uint16_t port, uint16_t value)
global outw
outw:
    mov ax, [esp + 8]   ; value
    mov dx, [esp + 4]   ; port
    out dx, ax
    ret

; void outl(uint16_t port, uint32_t value)
global outl
outl:
    mov eax, [esp + 8]  ; value
    mov dx, [esp + 4]   ; port
    out dx, eax
    ret

; uint8_t inb(uint16_t port)
global inb
inb:
    mov dx, [esp + 4]   ; port
    in al, dx
    ret

; uint16_t inw(uint16_t port)
global inw
inw:
    mov dx, [esp + 4]   ; port
    in ax, dx
    ret

; uint32_t inl(uint16_t port)
global inl
inl:
    mov dx, [esp + 4]   ; port
    in eax, dx
    ret

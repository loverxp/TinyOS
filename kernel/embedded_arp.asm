; embedded_arp.asm
global embedded_arp_start, embedded_arp_end
section .rodata
embedded_arp_start:
    incbin "build/user/arp.elf"
embedded_arp_end:

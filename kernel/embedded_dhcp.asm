; embedded_dhcp.asm
global embedded_dhcp_start, embedded_dhcp_end
section .rodata
embedded_dhcp_start:
    incbin "build/user/dhcp.elf"
embedded_dhcp_end:

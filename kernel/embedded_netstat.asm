; embedded_netstat.asm
global embedded_netstat_start, embedded_netstat_end
section .rodata
embedded_netstat_start:
    incbin "build/user/netstat.elf"
embedded_netstat_end:

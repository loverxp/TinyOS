; embedded_net_cmd.asm
global embedded_net_cmd_start, embedded_net_cmd_end
section .rodata
embedded_net_cmd_start:
    incbin "build/user/net.elf"
embedded_net_cmd_end:

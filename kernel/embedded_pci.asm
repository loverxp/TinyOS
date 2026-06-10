; embedded_pci.asm
global embedded_pci_start, embedded_pci_end
section .rodata
embedded_pci_start:
    incbin "build/user/pci.elf"
embedded_pci_end:

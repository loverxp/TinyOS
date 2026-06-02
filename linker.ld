/* Linker script for TinyOS */

ENTRY(start)

SECTIONS
{
    /* Begin at 1MB - standard for kernels */
    . = 0x100000;

    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        . = ALIGN(4K);
        kernel_end = .;
    }
}

/* user.ld - Linker script for user programs */

ENTRY(_start)

SECTIONS {
    /* Load address: 4 MB - well above kernel at 1 MB */
    . = 0x400000;

    .text : {
        /* crt0.o MUST be first - it contains _start, the entry point.
         * The loader jumps to the base address (0x400000), so _start
         * must be at the very beginning of the binary. */
        *crt0*(.text)
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    __bss_start = .;
    .bss : {
        *(.bss)
        *(COMMON)
    }
    __bss_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.debug*)
    }
}
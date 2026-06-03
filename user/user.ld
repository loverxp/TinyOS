/* user.ld - Linker script for user programs */

ENTRY(_start)

SECTIONS {
    /* Load address: 4 MB - well above kernel at 1 MB */
    . = 0x400000;

    .text : {
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
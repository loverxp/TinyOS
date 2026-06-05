# TinyOS GDB Debug Script
# Usage: i686-elf-gdb -x scripts/debug.gdb
# Or:    make gdb

# Connect to QEMU's GDB stub
target remote localhost:1234

# Load kernel symbol table
symbol-file build/tinyos.bin

# Set default memory size for the target
set remote memory-write-packet-size 1024

# Common breakpoints (uncomment as needed)
# b kernel_main
# b shell_cmd_handler
# b timer_handler
# b keyboard_handler
# b irq_common_stub
# b page_fault_handler
# b gp_fault_handler

# Useful display commands (run with `display/NUM` or `info display`)
# display/10wx 0x100000    # Watch kernel code region
# display/20wx 0xB8000     # Watch VGA text buffer

# Print kernel info on any break
define hook-stop
    printf "EIP=0x%08x ESP=0x%08x EBP=0x%08x\n", $eip, $esp, $ebp
end

# Custom: dump VGA text buffer
define vga
    x/80c 0xB8000
end
document vga
    Dump the first line of VGA text buffer
end

# Custom: show interrupt descriptor table
define idt
    # IDT is at a fixed address defined in interrupts.c
    # Typically around 0x100800-0x100880
    x/64hx 0x100800
end
document idt
    Dump IDT entries (64 bytes = first 8 entries)
end

# Custom: print kernel stack trace
define bt
    set $frame = $ebp
    while $frame != 0
        printf "EBP=0x%08x RET=0x%08x\n", $frame, *(unsigned int*)($frame+4)
        set $frame = *(unsigned int*)$frame
    end
end
document bt
    Print kernel stack backtrace by following EBP chain
end

# Custom: dump page directory
define pgd
    # Page directory at 0x1000 (phys), identity mapped
    printf "Page Directory at 0x1000:\n"
    x/16wx 0x1000
end
document pgd
    Print page directory entries (first 16)
end

# Let the CPU run
continue
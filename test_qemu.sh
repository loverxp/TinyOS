#!/bin/bash
# Build TinyOS
echo "Building TinyOS..."

# Assemble
./nasm.exe -f elf32 boot/boot.asm -o build/boot.o
./nasm.exe -f elf32 drivers/interrupts.asm -o build/interrupts.o
./nasm.exe -f elf32 drivers/gdt.asm -o build/gdt.o
./nasm.exe -f elf32 drivers/io.asm -o build/io.o

# Compile C files
export PATH="/d/Codes/Learning/TinyOS/tools/bin:$PATH"

i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/kernel.c -o build/kernel.o
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/vga.c -o build/vga.o
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/keyboard.c -o build/keyboard.o
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/timer.c -o build/timer.o
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/interrupts.c -o build/interrupts_c.o
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c lib/string.c -o build/string.o

# Link
./tools/i686-elf-ld.exe -T linker.ld -nostdlib -o tinyos.bin build/boot.o build/interrupts.o build/gdt.o build/io.o build/kernel.o build/vga.o build/keyboard.o build/timer.o build/interrupts_c.o build/string.o

echo "Build complete!"
echo ""
echo "To run in QEMU, use:"
echo '  "D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32'

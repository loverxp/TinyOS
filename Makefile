# TinyOS Makefile
# Builds a simple operating system that can run on QEMU

# Compiler and tools
CC = i686-elf-gcc
AS = nasm
LD = i686-elf-ld

# QEMU path
QEMU = "D:/Program Files/qemu/qemu-system-i386.exe"

# Compiler flags
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector \
         -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude

# Assembler flags
ASFLAGS = -f elf32

# Linker flags
LDFLAGS = -T linker.ld -nostdlib

# Source files
C_SOURCES = $(wildcard kernel/*.c) $(wildcard drivers/*.c) $(wildcard lib/*.c)
ASM_SOURCES = $(wildcard boot/*.asm) $(wildcard drivers/*.asm)

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)

# All object files
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Target
TARGET = tinyos.bin
ISO = tinyos.iso

# Default target
all: $(TARGET)

# Build the kernel binary
$(TARGET): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble assembly files
%.o: %.asm
	$(AS) $(ASFLAGS) -o $@ $<

# Create ISO image (optional, for CD boot)
iso: $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/
	echo 'menuentry "TinyOS" {' > iso/boot/grub/grub.cfg
	echo '    multiboot /boot/$(TARGET)' >> iso/boot/grub/grub.cfg
	echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

# Run in QEMU
run: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32

# Run in QEMU with serial output
run-debug: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -serial stdio

# Run in QEMU with no graphic (serial only)
run-serial: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -nographic

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET) $(ISO)
	rm -rf iso

# Rebuild
rebuild: clean all

.PHONY: all iso run run-debug run-serial clean rebuild

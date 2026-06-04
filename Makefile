# TinyOS Makefile
# Supports incremental builds and user program building.

# Tools (use full paths since they're not on PATH on Windows)
CC   = tools/bin/i686-elf-gcc.exe
AS   = nasm.exe
LD   = tools/i686-elf-ld.exe
ifeq (,$(wildcard $(LD)))
LD   = tools/bin/i686-elf-ld.exe
endif
QEMU = "D:/Program Files/qemu/qemu-system-i386.exe"

# User program tools
USER_CC = $(CC)
USER_AS = $(AS)
USER_LD = $(LD)
USER_OBJCOPY = tools/i686-elf-objcopy.exe
ifeq (,$(wildcard $(USER_OBJCOPY)))
USER_OBJCOPY = tools/bin/i686-elf-objcopy.exe
endif

# Flags
CFLAGS   = -m32 -mgeneral-regs-only -ffreestanding -O2 -Wall -Wextra -fno-exceptions \
           -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude
ASFLAGS  = -f elf32
LDFLAGS  = -T linker.ld -nostdlib

# Build output directory
BUILD = build

# Source files
C_SRCS    = kernel/kernel.c kernel/shell.c kernel/except.c kernel/gdt.c \
            kernel/tss.c kernel/pmm.c kernel/paging.c kernel/mm.c kernel/loader.c \
            drivers/vga.c drivers/keyboard.c drivers/timer.c drivers/interrupts.c \
            lib/string.c lib/stdio.c
ASM_SRCS  = boot/boot.asm drivers/interrupts.asm drivers/gdt.asm \
            drivers/io.asm kernel/user.asm kernel/embedded_user.asm \
            kernel/embedded_hello.asm

# Object files (all in build/; .asm -> _asm.o to avoid name collision with .c)
C_OBJS    = $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SRCS)))
ASM_OBJS  = $(patsubst %.asm,$(BUILD)/%_asm.o,$(notdir $(ASM_SRCS)))
OBJECTS   = $(ASM_OBJS) $(C_OBJS)

# User program files
USER_BINS  = build/user/gfxsnake.bin build/user/hello.bin

# Target
TARGET    = $(BUILD)/tinyos.bin

.PHONY: all user-programs run run-debug run-serial clean rebuild

# Default target: build user programs first, then kernel
all: user-programs $(TARGET)

# Build user programs
user-programs:
	cd user && .\build.bat

# Link
$(TARGET): $(OBJECTS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# embedded binaries depend on user binaries
$(BUILD)/embedded_user_asm.o: kernel/embedded_user.asm build/user/gfxsnake.bin | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_hello_asm.o: kernel/embedded_hello.asm build/user/hello.bin | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C files: map e.g. kernel/kernel.c -> build/kernel.o
$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: drivers/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: lib/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble ASM files: map e.g. boot/boot.asm -> build/boot_asm.o
$(BUILD)/%_asm.o: boot/%.asm | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/%_asm.o: drivers/%.asm | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/%_asm.o: kernel/%.asm | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

# Ensure build directory exists
$(BUILD):
	@-mkdir $(BUILD) 2>nul

# Run
run: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -vga std

run-debug: $(TARGET) | logs
	$(QEMU) -kernel $(TARGET) -m 32 -vga std -serial file:logs/serial.log

run-serial: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -vga std -nographic

# Ensure logs directory exists
logs:
	@-mkdir logs 2>nul

# Clean (Windows-compatible)
clean:
	-if exist $(BUILD) rmdir /S /Q $(BUILD)
	-if exist logs rmdir /S /Q logs

rebuild: clean all
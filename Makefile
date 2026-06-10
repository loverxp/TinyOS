# TinyOS Makefile
# Supports incremental builds and user program building.

# Tools (use full paths since they're not on PATH on Windows)
# Set machine-specific paths in local_config.mk (see local_config.example.mk)
-include local_config.mk

CC   = tools/bin/i686-elf-gcc.exe
AS   = nasm.exe
LD   = tools/i686-elf-ld.exe
ifeq (,$(wildcard $(LD)))
LD   = tools/bin/i686-elf-ld.exe
endif
QEMU = "$(QEMU_PATH)"

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
            kernel/scheduler.c kernel/ipc.c kernel/fat16.c kernel/net.c kernel/wm.c \
            kernel/webserver.c kernel/httpclient.c kernel/mbr.c kernel/vfs.c \
            kernel/signal.c \
            drivers/vga.c drivers/keyboard.c drivers/timer.c drivers/interrupts.c \
            drivers/ata.c drivers/pci.c drivers/ne2000.c drivers/serial.c \
            drivers/vbe.c drivers/framebuf.c drivers/mouse.c drivers/builtin_font.c \
            drivers/rtc.c \
            lib/string.c lib/stdio.c lib/prng.c lib/debug.c
ASM_SRCS  = boot/boot.asm drivers/interrupts.asm drivers/gdt.asm \
            drivers/io.asm kernel/user.asm kernel/embedded_user.asm \
            kernel/embedded_hello.asm kernel/embedded_echo.asm \
            kernel/embedded_clear.asm kernel/embedded_help.asm \
            kernel/embedded_forktest.asm \
            kernel/embedded_uptime.asm kernel/embedded_date.asm \
            kernel/embedded_rand.asm kernel/embedded_meminfo.asm \
            kernel/embedded_diskinfo.asm kernel/switch.asm

# Object files (all in build/; .asm -> _asm.o to avoid name collision with .c)
C_OBJS    = $(patsubst %.c,$(BUILD)/%.o,$(notdir $(C_SRCS)))
ASM_OBJS  = $(patsubst %.asm,$(BUILD)/%_asm.o,$(notdir $(ASM_SRCS)))
OBJECTS   = $(ASM_OBJS) $(C_OBJS)

# User program files
USER_BINS  = build/user/gfxsnake.bin build/user/hello.bin
USER_ELFS  = build/user/gfxsnake.elf build/user/hello.elf

# Target
TARGET    = $(BUILD)/tinyos.bin

.PHONY: all user-programs run run-debug run-serial run-gdb gdb clean rebuild disk

# Default target: build user programs first, then kernel
all: user-programs $(TARGET)

# Build user programs
user-programs:
	cd user && .\build.bat

# Create FAT16 disk image
disk:
	python scripts\mkfat16.py disk.img

# Link (write to temp then move to work around path translation issues)
$(TARGET): $(OBJECTS) | $(BUILD)
	$(LD) $(LDFLAGS) -o tinyos_tmp.bin $(OBJECTS)
	move /Y tinyos_tmp.bin $(TARGET) >nul 2>&1

# embedded binaries depend on user binaries
$(BUILD)/embedded_user_asm.o: kernel/embedded_user.asm build/user/gfxsnake.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_hello_asm.o: kernel/embedded_hello.asm build/user/hello.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_echo_asm.o: kernel/embedded_echo.asm build/user/echo.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_clear_asm.o: kernel/embedded_clear.asm build/user/clear.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_help_asm.o: kernel/embedded_help.asm build/user/help.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_forktest_asm.o: kernel/embedded_forktest.asm build/user/forktest.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_uptime_asm.o: kernel/embedded_uptime.asm build/user/uptime.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_date_asm.o: kernel/embedded_date.asm build/user/date.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_rand_asm.o: kernel/embedded_rand.asm build/user/rand.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_meminfo_asm.o: kernel/embedded_meminfo.asm build/user/meminfo.elf | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/embedded_diskinfo_asm.o: kernel/embedded_diskinfo.asm build/user/diskinfo.elf | $(BUILD)
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

# Run (VGA window + PS/2 keyboard, no serial redirect)
run: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -vga std -drive file=disk.img,format=raw,if=ide -netdev user,id=net0,hostfwd=tcp::9088-:80,hostfwd=udp::8888-:8888 -device ne2k_pci,netdev=net0

# Run with serial debug logging
run-debug: $(TARGET) | logs
	$(QEMU) -kernel $(TARGET) -m 32 -vga std -serial file:logs/serial.log -drive file=disk.img,format=raw,if=ide -netdev user,id=net0,hostfwd=tcp::9088-:80,hostfwd=udp::8888-:8888 -device ne2k_pci,netdev=net0

run-serial: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -vga std -nographic -drive file=disk.img,format=raw,if=ide -netdev user,id=net0,hostfwd=tcp::9088-:80,hostfwd=udp::8888-:8888 -device ne2k_pci,netdev=net0

# Run with GDB stub (wait for GDB on port 1234, CPU frozen at start)
run-gdb: $(TARGET)
	$(QEMU) -kernel $(TARGET) -m 32 -vga std -serial file:logs/serial.log -drive file=disk.img,format=raw,if=ide -netdev user,id=net0,hostfwd=tcp::9088-:80,hostfwd=udp::8888-:8888 -device ne2k_pci,netdev=net0 -s -S

# Launch GDB and connect to QEMU
GDB = tools/i686-elf-gdb.exe
gdb: $(TARGET)
	$(GDB) -x scripts/debug.gdb

# Ensure logs directory exists
logs:
	@-mkdir logs 2>nul

# Clean (Windows-compatible)
clean:
	-if exist $(BUILD) rmdir /S /Q $(BUILD)
	-if exist logs\serial.log del logs\serial.log

rebuild: clean all

@echo off
cd /d "%~dp0.."

REM ==========================================
REM    TinyOS Simple Build Script
REM ==========================================

REM Check for QEMU
set QEMU="D:\Program Files\qemu\qemu-system-i386.exe"
if not exist %QEMU% (
    echo ERROR: QEMU not found at D:\Program Files\qemu
    pause
    exit /b 1
)
echo [OK] QEMU

REM Check for NASM
set NASM=nasm.exe
if not exist %NASM% (
    echo ERROR: nasm.exe not found in current directory
    pause
    exit /b 1
)
echo [OK] NASM

REM Check for GCC
set GCC=tools\bin\i686-elf-gcc.exe
if not exist %GCC% (
    echo ERROR: i686-elf-gcc not found in tools\bin
    pause
    exit /b 1
)
echo [OK] GCC

REM Check for LD
set LD=tools\bin\i686-elf-ld.exe
if exist %LD% goto ld_found
set LD=tools\i686-elf-ld.exe
:ld_found
if not exist %LD% (
    echo ERROR: Linker not found
    pause
    exit /b 1
)
echo [OK] LD

REM Build user programs first
echo.
echo Building user programs...
cd user
call build.bat
cd ..
if errorlevel 1 (
    echo WARNING: User program build failed, continuing with kernel only...
)
echo.

REM Clean old build
if exist build rmdir /S /Q build
mkdir build

REM Assemble
echo Assembling...
%NASM% -f elf32 boot/boot.asm -o build\boot.o
%NASM% -f elf32 drivers/interrupts.asm -o build\interrupts.o
%NASM% -f elf32 drivers/gdt.asm -o build\gdt_asm.o
%NASM% -f elf32 drivers/io.asm -o build\io.o
%NASM% -f elf32 kernel/user.asm -o build\user.o
%NASM% -f elf32 kernel/embedded_user.asm -o build\embedded_user.o
echo [OK] Assembly

REM Compile C files
echo Compiling C files...
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/gdt.c -o build\gdt.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/tss.c -o build\tss.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/kernel.c -o build\kernel.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/except.c -o build\except.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/shell.c -o build\shell.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/pmm.c -o build\pmm.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/paging.c -o build\paging.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/mm.c -o build\mm.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/loader.c -o build\loader.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/vga.c -o build\vga.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/keyboard.c -o build\keyboard.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/timer.c -o build\timer.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/interrupts.c -o build\interrupts_c.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c lib/string.c -o build\string.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c lib/stdio.c -o build\stdio.o
echo [OK] C

REM Link
echo Linking...
%LD% -T linker.ld -nostdlib -o build\tinyos.bin build\boot.o build\interrupts.o build\gdt_asm.o build\gdt.o build\tss.o build\io.o build\kernel.o build\except.o build\shell.o build\pmm.o build\paging.o build\mm.o build\loader.o build\embedded_user.o build\user.o build\vga.o build\keyboard.o build\timer.o build\interrupts_c.o build\string.o build\stdio.o

echo.
echo Build complete!
echo.

REM Run
%QEMU% -kernel build\tinyos.bin -m 32
@echo off
chcp 65001 >nul

echo ==========================================
echo    TinyOS Build Script
echo ==========================================
echo.

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
if exist %LD% goto ld_found
echo ERROR: i686-elf-ld not found
pause
exit /b 1
:ld_found
echo [OK] LD

echo.
echo Building...

if not exist build mkdir build

REM Assemble
%NASM% -f elf32 boot/boot.asm -o build\boot.o
%NASM% -f elf32 drivers/interrupts.asm -o build\interrupts.o
%NASM% -f elf32 drivers/gdt.asm -o build\gdt_asm.o
%NASM% -f elf32 drivers/io.asm -o build\io.o
%NASM% -f elf32 kernel/user.asm -o build\user.o

REM Compile
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/gdt.c -o build\gdt.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/tss.c -o build\tss.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/kernel.c -o build\kernel.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/except.c -o build\except.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/shell.c -o build\shell.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/vga.c -o build\vga.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/keyboard.c -o build\keyboard.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/timer.c -o build\timer.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/interrupts.c -o build\interrupts_c.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/pmm.c -o build\pmm.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/mm.c -o build\mm.o
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c lib/string.c -o build\string.o

REM Link
%LD% -T linker.ld -nostdlib -o tinyos.bin build\boot.o build\interrupts.o build\gdt_asm.o build\gdt.o build\tss.o build\io.o build\kernel.o build\except.o build\shell.o build\pmm.o build\mm.o build\user.o build\vga.o build\keyboard.o build\timer.o build\interrupts_c.o build\string.o

echo.
echo Build complete!
echo.

REM Run QEMU
echo Starting QEMU...
echo Click inside window to type. Ctrl+Alt to release mouse.
echo.
%QEMU% -kernel tinyos.bin -m 32

echo.
pause

@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo ==========================================
echo    TinyOS Build Script for Windows
echo ==========================================
echo.

REM Set paths
set QEMU_PATH=D:\Program Files\qemu
set NASM_PATH=C:\nasm
set GCC_PATH=C:\i686-elf\bin

REM Check for required tools
echo Checking for required tools...

REM Check for NASM
where nasm >nul 2>nul
if %errorlevel% neq 0 (
    if exist "%NASM_PATH%\nasm.exe" (
        set "PATH=%NASM_PATH%;%PATH%"
    ) else (
        echo ERROR: NASM not found! Please install NASM or add it to PATH.
        echo Download from: https://www.nasm.us/
        pause
        exit /b 1
    )
)
echo [OK] NASM found

REM Check for i686-elf-gcc
where i686-elf-gcc >nul 2>nul
if %errorlevel% neq 0 (
    if exist "%GCC_PATH%\i686-elf-gcc.exe" (
        set "PATH=%GCC_PATH%;%PATH%"
    ) else (
        echo ERROR: i686-elf-gcc not found! Please install the cross-compiler.
        echo You can use MSYS2 or build it from source.
        pause
        exit /b 1
    )
)
echo [OK] i686-elf-gcc found

REM Check for i686-elf-ld
where i686-elf-ld >nul 2>nul
if %errorlevel% neq 0 (
    if exist "%GCC_PATH%\i686-elf-ld.exe" (
        set "PATH=%GCC_PATH%;%PATH%"
    ) else (
        echo ERROR: i686-elf-ld not found! Please install the cross-compiler.
        pause
        exit /b 1
    )
)
echo [OK] i686-elf-ld found

REM Check for QEMU
if not exist "%QEMU_PATH%\qemu-system-i386.exe" (
    echo ERROR: QEMU not found at %QEMU_PATH%
    echo Please install QEMU or update the path in this script.
    pause
    exit /b 1
)
echo [OK] QEMU found

echo.
echo ==========================================
echo    Building TinyOS...
echo ==========================================
echo.

REM Create output directory
if not exist build mkdir build

REM Assemble boot.asm
echo [1/7] Assembling boot.asm...
nasm -f elf32 boot/boot.asm -o build/boot.o
if %errorlevel% neq 0 goto error

REM Assemble interrupts.asm
echo [2/7] Assembling interrupts.asm...
nasm -f elf32 drivers/interrupts.asm -o build/interrupts.o
if %errorlevel% neq 0 goto error

REM Assemble gdt.asm
echo [3/7] Assembling gdt.asm...
nasm -f elf32 drivers/gdt.asm -o build/gdt.o
if %errorlevel% neq 0 goto error

REM Compile kernel.c
echo [4/7] Compiling kernel.c...
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c kernel/kernel.c -o build/kernel.o
if %errorlevel% neq 0 goto error

REM Compile vga.c
echo [5/7] Compiling vga.c...
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/vga.c -o build/vga.o
if %errorlevel% neq 0 goto error

REM Compile keyboard.c
echo [6/7] Compiling keyboard.c...
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/keyboard.c -o build/keyboard.o
if %errorlevel% neq 0 goto error

REM Compile timer.c
echo [7/7] Compiling timer.c...
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/timer.c -o build/timer.o
if %errorlevel% neq 0 goto error

REM Compile interrupts.c
echo [8/8] Compiling interrupts.c...
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c drivers/interrupts.c -o build/interrupts_c.o
if %errorlevel% neq 0 goto error

REM Compile string.c
echo [9/9] Compiling string.c...
i686-elf-gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -Iinclude -c lib/string.c -o build/string.o
if %errorlevel% neq 0 goto error

REM Link everything
echo.
echo Linking kernel...
i686-elf-ld -T linker.ld -nostdlib -o tinyos.bin build/boot.o build/interrupts.o build/gdt.o build/kernel.o build/vga.o build/keyboard.o build/timer.o build/interrupts_c.o build/string.o
if %errorlevel% neq 0 goto error

echo.
echo ==========================================
echo    Build successful!
echo ==========================================
echo.

REM Check if multiboot header is valid
echo Checking multiboot header...
for /f "delims=" %%a in ('"%QEMU_PATH%\qemu-system-i386.exe" -kernel tinyos.bin -m 32 -display none -device isa-debug-exit,iobase=0xf4,iosize=0x04 2^>^&1') do (
    echo %%a | findstr /C:"multiboot" >nul && (
        echo [WARNING] Multiboot error detected
    )
)

echo.
echo Starting QEMU...
echo.
"%QEMU_PATH%\qemu-system-i386.exe" -kernel tinyos.bin -m 32

goto end

:error
echo.
echo ==========================================
echo    BUILD FAILED!
echo ==========================================
echo.
pause
exit /b 1

:end
echo.
echo QEMU closed.
pause

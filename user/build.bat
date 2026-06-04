@echo off
REM Build user programs for TinyOS
REM Requires: i686-elf-gcc, nasm, objcopy

set GCC=..\tools\bin\i686-elf-gcc.exe
set LD=..\tools\bin\i686-elf-ld.exe
if not exist %LD% set LD=..\tools\i686-elf-ld.exe
set AS=..\nasm.exe
set OBJCOPY=..\tools\bin\i686-elf-objcopy.exe
if not exist %OBJCOPY% set OBJCOPY=..\tools\i686-elf-objcopy.exe

set OUT=..\build\user
if not exist %OUT% mkdir %OUT%

set CFLAGS=-m32 -mgeneral-regs-only -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -I libc
set LDFLAGS=-T user.ld -nostdlib

echo Building user: crt0.o
%AS% -f elf32 crt0.s -o %OUT%\crt0.o
if errorlevel 1 exit /b 1

echo Building libc...
%GCC% %CFLAGS% -c libc\stdio.c -o %OUT%\libc_stdio.o
if errorlevel 1 exit /b 1

%GCC% %CFLAGS% -c libc\stdlib.c -o %OUT%\libc_stdlib.o
if errorlevel 1 exit /b 1

%GCC% %CFLAGS% -c libc\string.c -o %OUT%\libc_string.o
if errorlevel 1 exit /b 1

set LIBC=%OUT%\libc_stdio.o %OUT%\libc_stdlib.o %OUT%\libc_string.o

echo Building user: hello
%GCC% %CFLAGS% -c hello.c -o %OUT%\hello.o
if errorlevel 1 exit /b 1

echo Linking: hello.elf
%LD% %LDFLAGS% -o %OUT%\hello.elf %OUT%\hello.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: hello.bin
%OBJCOPY% -O binary %OUT%\hello.elf %OUT%\hello.bin
if errorlevel 1 exit /b 1

echo Building user: gfxsnake
%GCC% %CFLAGS% -c gfxsnake.c -o %OUT%\gfxsnake.o
if errorlevel 1 exit /b 1

echo Linking: gfxsnake.elf
%LD% %LDFLAGS% -o %OUT%\gfxsnake.elf %OUT%\gfxsnake.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: gfxsnake.bin
%OBJCOPY% -O binary %OUT%\gfxsnake.elf %OUT%\gfxsnake.bin
if errorlevel 1 exit /b 1

echo Done: hello.bin, gfxsnake.bin
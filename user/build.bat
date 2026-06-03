@echo off
REM Build user programs for TinyOS
REM Requires: i686-elf-gcc, nasm, objcopy

set GCC=..\tools\bin\i686-elf-gcc.exe
set LD=..\tools\bin\i686-elf-ld.exe
if not exist %LD% set LD=..\tools\i686-elf-ld.exe
set AS=..\nasm.exe
set OBJCOPY=..\tools\bin\i686-elf-objcopy.exe
if not exist %OBJCOPY% set OBJCOPY=..\tools\i686-elf-objcopy.exe

if not exist programs mkdir programs

echo Building user: crt0.o
%AS% -f elf32 crt0.s -o programs\crt0.o
if errorlevel 1 exit /b 1

echo Building user: hello
%GCC% -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -I..\include -c hello.c -o programs\hello.o
if errorlevel 1 exit /b 1

echo Linking: hello.elf
%LD% -T user.ld -nostdlib -o programs\hello.elf programs\hello.o programs\crt0.o
if errorlevel 1 exit /b 1

echo Converting: hello.bin
%OBJCOPY% -O binary programs\hello.elf programs\hello.bin
if errorlevel 1 exit /b 1

echo Done: user\programs\hello.bin (% ~z programs\hello.bin bytes)
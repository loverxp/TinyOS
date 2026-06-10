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

set CFLAGS=-m32 -mgeneral-regs-only -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -nostdlib -nostdinc -fno-pic -fno-pie -I include -I libc
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

%GCC% %CFLAGS% -c libc\errno.c -o %OUT%\libc_errno.o
if errorlevel 1 exit /b 1

%GCC% %CFLAGS% -c libc\termios.c -o %OUT%\libc_termios.o
if errorlevel 1 exit /b 1

set LIBC=%OUT%\libc_stdio.o %OUT%\libc_stdlib.o %OUT%\libc_string.o %OUT%\libc_errno.o %OUT%\libc_termios.o

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

echo Building user: echo
%GCC% %CFLAGS% -c apps\echo.c -o %OUT%\echo.o
if errorlevel 1 exit /b 1

echo Linking: echo.elf
%LD% %LDFLAGS% -o %OUT%\echo.elf %OUT%\echo.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: echo.bin
%OBJCOPY% -O binary %OUT%\echo.elf %OUT%\echo.bin
if errorlevel 1 exit /b 1

echo Building user: clear
%GCC% %CFLAGS% -c apps\clear.c -o %OUT%\clear.o
if errorlevel 1 exit /b 1

echo Linking: clear.elf
%LD% %LDFLAGS% -o %OUT%\clear.elf %OUT%\clear.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: clear.bin
%OBJCOPY% -O binary %OUT%\clear.elf %OUT%\clear.bin
if errorlevel 1 exit /b 1

echo Building user: help
%GCC% %CFLAGS% -c apps\help.c -o %OUT%\help.o
if errorlevel 1 exit /b 1

echo Linking: help.elf
%LD% %LDFLAGS% -o %OUT%\help.elf %OUT%\help.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: help.bin
%OBJCOPY% -O binary %OUT%\help.elf %OUT%\help.bin
if errorlevel 1 exit /b 1

echo Building user: forktest
%GCC% %CFLAGS% -c apps\forktest.c -o %OUT%\forktest.o
if errorlevel 1 exit /b 1

echo Linking: forktest.elf
%LD% %LDFLAGS% -o %OUT%\forktest.elf %OUT%\forktest.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: forktest.bin
%OBJCOPY% -O binary %OUT%\forktest.elf %OUT%\forktest.bin
if errorlevel 1 exit /b 1

echo Building user: uptime
%GCC% %CFLAGS% -c apps\uptime.c -o %OUT%\uptime.o
if errorlevel 1 exit /b 1

echo Linking: uptime.elf
%LD% %LDFLAGS% -o %OUT%\uptime.elf %OUT%\uptime.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: uptime.bin
%OBJCOPY% -O binary %OUT%\uptime.elf %OUT%\uptime.bin
if errorlevel 1 exit /b 1

echo Building user: date
%GCC% %CFLAGS% -c apps\date.c -o %OUT%\date.o
if errorlevel 1 exit /b 1

echo Linking: date.elf
%LD% %LDFLAGS% -o %OUT%\date.elf %OUT%\date.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: date.bin
%OBJCOPY% -O binary %OUT%\date.elf %OUT%\date.bin
if errorlevel 1 exit /b 1

echo Building user: rand
%GCC% %CFLAGS% -c apps\rand.c -o %OUT%\rand.o
if errorlevel 1 exit /b 1

echo Linking: rand.elf
%LD% %LDFLAGS% -o %OUT%\rand.elf %OUT%\rand.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: rand.bin
%OBJCOPY% -O binary %OUT%\rand.elf %OUT%\rand.bin
if errorlevel 1 exit /b 1

echo Building user: meminfo
%GCC% %CFLAGS% -c apps\meminfo.c -o %OUT%\meminfo.o
if errorlevel 1 exit /b 1

echo Linking: meminfo.elf
%LD% %LDFLAGS% -o %OUT%\meminfo.elf %OUT%\meminfo.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: meminfo.bin
%OBJCOPY% -O binary %OUT%\meminfo.elf %OUT%\meminfo.bin
if errorlevel 1 exit /b 1

echo Building user: diskinfo
%GCC% %CFLAGS% -c apps\diskinfo.c -o %OUT%\diskinfo.o
if errorlevel 1 exit /b 1

echo Linking: diskinfo.elf
%LD% %LDFLAGS% -o %OUT%\diskinfo.elf %OUT%\diskinfo.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: diskinfo.bin
%OBJCOPY% -O binary %OUT%\diskinfo.elf %OUT%\diskinfo.bin
if errorlevel 1 exit /b 1

echo Building user: ls
%GCC% %CFLAGS% -c apps\ls.c -o %OUT%\ls.o
if errorlevel 1 exit /b 1

echo Linking: ls.elf
%LD% %LDFLAGS% -o %OUT%\ls.elf %OUT%\ls.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: ls.bin
%OBJCOPY% -O binary %OUT%\ls.elf %OUT%\ls.bin
if errorlevel 1 exit /b 1

echo Building user: cat
%GCC% %CFLAGS% -c apps\cat.c -o %OUT%\cat.o
if errorlevel 1 exit /b 1

echo Linking: cat.elf
%LD% %LDFLAGS% -o %OUT%\cat.elf %OUT%\cat.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: cat.bin
%OBJCOPY% -O binary %OUT%\cat.elf %OUT%\cat.bin
if errorlevel 1 exit /b 1

echo Building user: more
%GCC% %CFLAGS% -c apps\more.c -o %OUT%\more.o
if errorlevel 1 exit /b 1

echo Linking: more.elf
%LD% %LDFLAGS% -o %OUT%\more.elf %OUT%\more.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: more.bin
%OBJCOPY% -O binary %OUT%\more.elf %OUT%\more.bin
if errorlevel 1 exit /b 1

echo Building user: write
%GCC% %CFLAGS% -c apps\write.c -o %OUT%\write.o
if errorlevel 1 exit /b 1

echo Linking: write.elf
%LD% %LDFLAGS% -o %OUT%\write.elf %OUT%\write.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: write.bin
%OBJCOPY% -O binary %OUT%\write.elf %OUT%\write.bin
if errorlevel 1 exit /b 1

echo Building user: rm
%GCC% %CFLAGS% -c apps\rm.c -o %OUT%\rm.o
if errorlevel 1 exit /b 1

echo Linking: rm.elf
%LD% %LDFLAGS% -o %OUT%\rm.elf %OUT%\rm.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: rm.bin
%OBJCOPY% -O binary %OUT%\rm.elf %OUT%\rm.bin
if errorlevel 1 exit /b 1

echo Building user: mkdir
%GCC% %CFLAGS% -c apps\mkdir.c -o %OUT%\mkdir.o
if errorlevel 1 exit /b 1

echo Linking: mkdir.elf
%LD% %LDFLAGS% -o %OUT%\mkdir.elf %OUT%\mkdir.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: mkdir.bin
%OBJCOPY% -O binary %OUT%\mkdir.elf %OUT%\mkdir.bin
if errorlevel 1 exit /b 1

echo Building user: rmdir
%GCC% %CFLAGS% -c apps\rmdir.c -o %OUT%\rmdir.o
if errorlevel 1 exit /b 1

echo Linking: rmdir.elf
%LD% %LDFLAGS% -o %OUT%\rmdir.elf %OUT%\rmdir.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: rmdir.bin
%OBJCOPY% -O binary %OUT%\rmdir.elf %OUT%\rmdir.bin
if errorlevel 1 exit /b 1

echo Building user: ping
%GCC% %CFLAGS% -c apps\ping.c -o %OUT%\ping.o
if errorlevel 1 exit /b 1

echo Linking: ping.elf
%LD% %LDFLAGS% -o %OUT%\ping.elf %OUT%\ping.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: ping.bin
%OBJCOPY% -O binary %OUT%\ping.elf %OUT%\ping.bin
if errorlevel 1 exit /b 1

echo Building user: arp
%GCC% %CFLAGS% -c apps\arp.c -o %OUT%\arp.o
if errorlevel 1 exit /b 1

echo Linking: arp.elf
%LD% %LDFLAGS% -o %OUT%\arp.elf %OUT%\arp.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: arp.bin
%OBJCOPY% -O binary %OUT%\arp.elf %OUT%\arp.bin
if errorlevel 1 exit /b 1

echo Building user: net
%GCC% %CFLAGS% -c apps\net.c -o %OUT%\net.o
if errorlevel 1 exit /b 1

echo Linking: net.elf
%LD% %LDFLAGS% -o %OUT%\net.elf %OUT%\net.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: net.bin
%OBJCOPY% -O binary %OUT%\net.elf %OUT%\net.bin
if errorlevel 1 exit /b 1

echo Building user: netstat
%GCC% %CFLAGS% -c apps\netstat.c -o %OUT%\netstat.o
if errorlevel 1 exit /b 1

echo Linking: netstat.elf
%LD% %LDFLAGS% -o %OUT%\netstat.elf %OUT%\netstat.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: netstat.bin
%OBJCOPY% -O binary %OUT%\netstat.elf %OUT%\netstat.bin
if errorlevel 1 exit /b 1

echo Building user: dhcp
%GCC% %CFLAGS% -c apps\dhcp.c -o %OUT%\dhcp.o
if errorlevel 1 exit /b 1

echo Linking: dhcp.elf
%LD% %LDFLAGS% -o %OUT%\dhcp.elf %OUT%\dhcp.o %OUT%\crt0.o %LIBC%
if errorlevel 1 exit /b 1

echo Converting: dhcp.bin
%OBJCOPY% -O binary %OUT%\dhcp.elf %OUT%\dhcp.bin
if errorlevel 1 exit /b 1

echo Done: hello.bin, gfxsnake.bin, echo.bin, clear.bin, help.bin, forktest.bin, uptime.bin, date.bin, rand.bin, meminfo.bin, diskinfo.bin, ls.bin, cat.bin, more.bin, write.bin, rm.bin, mkdir.bin, rmdir.bin, ping.bin, arp.bin, net.bin, netstat.bin, dhcp.bin
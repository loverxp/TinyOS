@echo off
cd /d "%~dp0.."
echo Starting TinyOS with exception handling...
"D:\Program Files\qemu\qemu-system-i386.exe" -kernel build\tinyos.bin -m 32
pause
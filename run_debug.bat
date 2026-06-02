@echo off
echo Starting TinyOS with debug output...
"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32 -serial stdio
pause

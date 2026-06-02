@echo off
echo ==========================================
echo    TinyOS - Exception Test
echo ==========================================
echo.
echo This will trigger a division by zero exception.
echo Check the output below for exception details.
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32 -serial stdio

echo.
echo QEMU exited.
pause

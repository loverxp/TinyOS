@echo off
echo ==========================================
echo    TinyOS Launcher
echo ==========================================
echo.
echo Starting TinyOS...
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32

echo.
echo QEMU exited.
pause

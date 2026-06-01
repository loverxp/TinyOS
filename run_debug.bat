@echo off
echo ==========================================
echo    TinyOS Debug Launcher
echo ==========================================
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32 -serial stdio

echo.
echo QEMU exited.
pause

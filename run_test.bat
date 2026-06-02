@echo off
echo ==========================================
echo    TinyOS - Exception Demo
echo ==========================================
echo.
echo Press 'E' key in the QEMU window to trigger
echo a Division By Zero exception demo.
echo.
echo Serial debug output will appear here.
echo.
echo NOTE: Click inside QEMU window first to
echo capture keyboard input. Ctrl+Alt to release.
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32 -serial stdio

echo.
echo QEMU exited.
pause
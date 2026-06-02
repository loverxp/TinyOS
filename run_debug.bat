@echo off
echo ==========================================
echo    TinyOS - Serial Debug Mode
echo ==========================================
echo.
echo Serial debug output will appear here.
echo Check the QEMU window for VGA display.
echo.
echo NOTE: Click inside QEMU window first to
echo capture keyboard input. Ctrl+Alt to release.
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel tinyos.bin -m 32 -serial stdio

echo.
echo QEMU exited.
pause
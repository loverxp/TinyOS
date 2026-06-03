@echo off
cd /d "%~dp0.."

REM Ensure logs directory exists
if not exist logs mkdir logs

echo ==========================================
echo    TinyOS - Serial Debug Mode
echo ==========================================
echo.
echo Serial debug output will be saved to logs\serial.log
echo Check the QEMU window for VGA display.
echo.
echo NOTE: Click inside QEMU window first to
echo capture keyboard input. Ctrl+Alt to release.
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel build\tinyos.bin -m 32 -serial file:logs\serial.log

echo.
echo QEMU exited.
echo Serial log saved to: logs\serial.log
pause
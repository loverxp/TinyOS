@echo off
cd /d "%~dp0.."

REM Ensure logs directory exists
if not exist logs mkdir logs

echo ==========================================
echo    TinyOS - Exception Demo
echo ==========================================
echo.
echo Press 'E' key in the QEMU window to trigger
echo a Division By Zero exception demo.
echo.
echo Serial debug output will be saved to logs\serial.log
echo.
echo NOTE: Click inside QEMU window first to
echo capture keyboard input. Ctrl+Alt to release.
echo.
echo.

"D:\Program Files\qemu\qemu-system-i386.exe" -kernel build\tinyos.bin -m 32 -serial file:logs\serial.log

echo.
echo QEMU exited.
echo Serial log saved to: logs\serial.log
pause
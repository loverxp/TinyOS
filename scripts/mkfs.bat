@echo off
REM Create a 16MB FAT16 disk image for TinyOS
REM Requires: Python 3.x (to create a valid FAT16 image without external tools)

echo Creating 16MB FAT16 disk image...

python scripts\mkfat16.py disk.img
if %ERRORLEVEL% NEQ 0 (
    echo Python not found or script failed.
    echo Creating raw image with fsutil instead...
    fsutil file createnew disk.img 16777216
    echo NOTE: disk.img is empty. Format it with a FAT16 tool or use Python script.
)

echo Done: disk.img

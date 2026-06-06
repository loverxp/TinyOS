$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = "D:\Program Files\qemu\qemu-system-i386.exe"
$psi.Arguments = "-kernel build/tinyos.bin -m 32 -vga std -nographic -serial file:logs/serial.log -drive file=disk.img,format=raw,if=ide"
$psi.UseShellExecute = $false
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$p = [System.Diagnostics.Process]::Start($psi)
Start-Sleep -Seconds 4
$p.StandardInput.WriteLine("forktest")
Start-Sleep -Seconds 12
$p.Kill()

# Flash an openvela/NuttX board reliably.
#
# The board's ROM bootloader only listens for a very short window (~2s) after a
# cold power-up, so a plain `sftool write_flash` usually misses it and fails with
# "Failed to download stub".  This script waits for the operator to unplug the
# board and, the instant the port reappears, fires the flash - which is the only
# timing that reliably works.
#
# Usage: adjust $sftool, $bin and $port, then run it and (re)plug the board USB.

$sftool = "C:\path\to\sftool.exe"
$bin    = "C:\path\to\nuttx.bin@0x12010000"
$port   = "COM5"
$chip   = "SF32LB52"

function Port-Present { [System.IO.Ports.SerialPort]::GetPortNames() -contains $port }

$deadline = (Get-Date).AddSeconds(600)

Write-Output "waiting for $port to disappear (unplug the board)..."
while ((Get-Date) -lt $deadline -and (Port-Present)) { Start-Sleep -Milliseconds 200 }
if ((Get-Date) -ge $deadline) { Write-Output "timeout waiting for unplug"; exit 1 }

Write-Output "unplugged. Waiting for replug - will fire immediately..."
while ((Get-Date) -lt $deadline -and -not (Port-Present)) { Start-Sleep -Milliseconds 50 }
Write-Output "$port back at $(Get-Date -Format HH:mm:ss) - flashing now"

$ok = $false
for ($i = 1; $i -le 12; $i++) {
    & $sftool -c $chip -m nor -p $port -b 1000000 `
        --before default_reset --after soft_reset `
        --connect-attempts 2 write_flash $bin 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Output "FLASH OK on attempt $i"; $ok = $true; break }
    Start-Sleep -Milliseconds 300
}
if (-not $ok) { Write-Output "FAILED after 12 attempts" }

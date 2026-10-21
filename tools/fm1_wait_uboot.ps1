# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
# Wait for the FM-1's backup mode (the chip's USB boot: a USB disk "WL82 UBOOT1.00"), then start MvaveFM1Unbricker
# at once. Entered from a running firmware (OCT- + OCT+ held 5 s, the SysEx key, the console's "uboot yes"), backup
# mode lasts only ~13 s unless a tool connects; started by hand, the unbricker's one scan usually misses it.
# Nothing here opens or writes the disk; the unbricker does what its arguments say (default: a read-only backup).
#
#   In an administrator PowerShell:
#   powershell -ExecutionPolicy Bypass -File $env:USERPROFILE\fm1-dev\strata\tools\fm1_wait_uboot.ps1
#   ... then hold OCT- + OCT+ for 5 s (Cancel Windows' offer to format the disk).
#   Other unbricker commands: add them at the end, e.g.  ... fm1_wait_uboot.ps1 info
[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$Unbricker = "$env:USERPROFILE\fm1-dev\ref\MvaveFM1Unbricker",
    [double]$Minutes = 3,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ToolArgs
)
if (-not $ToolArgs) { $ToolArgs = @("backup", "$env:USERPROFILE\fm1-backup") }
if (-not (Test-Path (Join-Path $Unbricker "fm1_unbrick.py"))) {
    Write-Host "No fm1_unbrick.py in $Unbricker"
    exit 1
}
Set-Location $Unbricker
Write-Host "Waiting for the WL82 UBOOT1.00 disk (up to $Minutes min)."
Write-Host "Now hold OCT- and OCT+ together for 5 s. If Windows offers to format a disk: Cancel."
$deadline = (Get-Date).AddMinutes($Minutes)
while ((Get-Date) -lt $deadline) {
    $disk = Get-CimInstance Win32_DiskDrive -Filter "Model LIKE '%WL82 UBOOT1.00%'" -ErrorAction SilentlyContinue |
        Where-Object { $_.InterfaceType -eq "USB" } | Select-Object -First 1
    if ($disk) {
        Write-Host ("Found it: disk {0} ({1}). Starting: py fm1_unbrick.py {2}" -f $disk.Index, $disk.Model, ($ToolArgs -join " "))
        & py fm1_unbrick.py @ToolArgs
        exit $LASTEXITCODE
    }
    Start-Sleep -Milliseconds 200
}
Write-Host "No WL82 UBOOT1.00 disk appeared. Nothing was started."
exit 1

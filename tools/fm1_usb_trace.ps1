# SPDX-License-Identifier: GPL-3.0-only
# Windows' own USB trace (the USBXHCI, UCX and USBHUB3 providers, keyword Default: transfer and enumeration
# events with their status codes, no data payloads), to see how the FM-1's transfers end: completed, refused,
# broken on the wire, or never answered. Nothing is sent to the FM-1. Needs an administrator PowerShell.
#
#   powershell -ExecutionPolicy Bypass -File $env:USERPROFILE\fm1-dev\strata\tools\fm1_usb_trace.ps1 start
#   ... reproduce (the USB check page's steps 2 and 4, then OCT- + OCT+ held 5 s) ...
#   powershell -ExecutionPolicy Bypass -File $env:USERPROFILE\fm1-dev\strata\tools\fm1_usb_trace.ps1 stop
#
# The trace: $env:USERPROFILE\fm1-usbtrace\fm1usb.etl
param(
    [ValidateSet("start", "stop")][string]$Action = "start",
    [string]$Out = "$env:USERPROFILE\fm1-usbtrace"
)
$name = "fm1usb"
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    Write-Host "Run this from an administrator PowerShell (right-click Start, Terminal (Admin))."
    exit 1
}
if ($Action -eq "start") {
    New-Item -ItemType Directory -Force -Path $Out | Out-Null
    logman stop $name 2>$null | Out-Null
    logman delete $name 2>$null | Out-Null
    logman create trace $name -o (Join-Path $Out "fm1usb.etl") -nb 128 640 -bs 128 -ow | Out-Null
    foreach ($p in "Microsoft-Windows-USB-USBXHCI", "Microsoft-Windows-USB-UCX", "Microsoft-Windows-USB-USBHUB3") {
        logman update trace $name -p $p "(Default)" | Out-Null
    }
    logman start $name
    if ($LASTEXITCODE -eq 0) { Write-Host "Tracing. Now do the steps, then run this script again with: stop" }
    exit $LASTEXITCODE
}
logman stop $name
logman delete $name | Out-Null
$etl = Join-Path $Out "fm1usb.etl"
if (Test-Path $etl) { Write-Host ("Stopped. Trace: {0} ({1:N0} bytes)" -f $etl, (Get-Item $etl).Length) }
else { Write-Host "Stopped, but no trace file was written." }

#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$IdfProfile = 'C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1'
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
if (-not (Test-Path -LiteralPath $IdfProfile)) {
    throw 'ESP-IDF activation script not found. Pass -IdfProfile with your installation path.'
}
. $IdfProfile
Push-Location -LiteralPath $PSScriptRoot
try {
    idf.py --preview -DIDF_TARGET=esp32s31 build
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
}
finally { Pop-Location }

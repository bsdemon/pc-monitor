#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateScript({
        $parsedAddress = $null
        if (-not [System.Net.IPAddress]::TryParse($_, [ref]$parsedAddress) -or
            $parsedAddress.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
            throw 'BindAddress must be an IPv4 address.'
        }
        $true
    })]
    [string]$BindAddress = '0.0.0.0',

    [ValidateRange(1, 65535)]
    [int]$Port = 8080
)

$ErrorActionPreference = 'Stop'

try {
    $uvCommand = Get-Command uv -CommandType Application -ErrorAction SilentlyContinue
    if (-not $uvCommand) {
        throw 'uv was not found. Install uv and open a new PowerShell window: https://docs.astral.sh/uv/getting-started/installation/'
    }

    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    $isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host 'Requesting administrator access for hardware sensors...'
        # Encode the command so paths with spaces or apostrophes survive UAC relaunch.
        $quotedScript = $PSCommandPath.Replace("'", "''")
        $command = "& '$quotedScript' -BindAddress '$BindAddress' -Port $Port"
        $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
        Start-Process -FilePath "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
            -Verb RunAs -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encoded) `
            -WindowStyle Normal
        # Let the original CMD/shortcut window close after the elevated launch.
        exit 0
    }

    Push-Location -LiteralPath $PSScriptRoot
    try {
        Write-Host 'Starting PC Monitor. Press Ctrl+C to stop.'
        Write-Host "Dashboard: http://localhost:$Port"
        if ($BindAddress -ne '127.0.0.1') {
            Write-Host "ESP32 API: http://<PC-LAN-IP>:$Port/api/metrics"
        }
        & $uvCommand.Source run --locked pc-monitor serve --host $BindAddress --port $Port
        if ($LASTEXITCODE -ne 0) {
            throw "PC Monitor exited with code $LASTEXITCODE. Check the messages above."
        }
    }
    finally {
        Pop-Location
    }
}
catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Read-Host 'Press Enter to close'
    exit 1
}

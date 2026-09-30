[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $DllPath,

    [switch] $Unregister
)

# Keep this file ASCII: Windows PowerShell 5 reads BOM-less UTF-8 as ANSI.
$ErrorActionPreference = 'Stop'
$resolvedDll = (Resolve-Path -LiteralPath $DllPath).Path

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-Administrator)) {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -DllPath "{1}"' `
        -f $PSCommandPath, $resolvedDll
    if ($Unregister) { $arguments += ' -Unregister' }
    $process = Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments `
        -Verb RunAs -Wait -PassThru
    exit $process.ExitCode
}

if (-not $Unregister) {
    # AppContainer hosts (Start/Search, Store apps) must be able to read both
    $icacls = Join-Path ([Environment]::SystemDirectory) 'icacls.exe'
    $moduleDirectory = Split-Path -Parent $resolvedDll
    foreach ($file in @($resolvedDll, (Join-Path $moduleDirectory 'ChiaKeySource.db'))) {
        if (Test-Path -LiteralPath $file -PathType Leaf) {
            & $icacls $file /grant '*S-1-15-2-1:(RX)' /q | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "icacls failed for $file" }
        }
    }
}

# no unregister before an upgrade: it raises a Simplified Chinese dictionary notice
$regsvr32 = Join-Path $env:SystemRoot 'System32\regsvr32.exe'
$argumentString = '/s '
if ($Unregister) { $argumentString += '/u ' }
$argumentString += '"{0}"' -f $resolvedDll
$registration = Start-Process -FilePath $regsvr32 -ArgumentList $argumentString `
    -WindowStyle Hidden -Wait -PassThru
if ($registration.ExitCode -ne 0) {
    throw "regsvr32 failed with exit code $($registration.ExitCode)."
}

$operation = if ($Unregister) { 'Unregistered' } else { 'Registered' }
Write-Host "$operation ChiaKey TSF: $resolvedDll"

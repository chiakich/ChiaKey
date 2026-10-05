# Read-only collection: no process injection, debugger, settings changes or network calls.
param(
    [string] $OutputPath = (Join-Path $env:TEMP ('ChiaKey-Game-Diagnostics-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.json')),
    [string] $ProcessPattern = 'League|Riot|VALORANT',
    [ValidateRange(1, 1440)] [int] $RecentMinutes = 30
)
$ErrorActionPreference = 'Stop'
$os = Get-CimInstance Win32_OperatingSystem
$profiles = @()
foreach ($tipPath in @(
    'HKLM:\SOFTWARE\Microsoft\CTF\TIP\{25CF860B-9F43-4247-BF94-1077848E5D84}',
    'HKLM:\SOFTWARE\WOW6432Node\Microsoft\CTF\TIP\{25CF860B-9F43-4247-BF94-1077848E5D84}'
)) {
    if (Test-Path $tipPath) {
        foreach ($key in Get-ChildItem $tipPath -Recurse) {
            $value = Get-ItemProperty $key.PSPath
            if ($value.IconFile) {
                $signature = Get-AuthenticodeSignature -LiteralPath $value.IconFile -ErrorAction SilentlyContinue
                $profiles += [pscustomobject]@{
                    RegistryPath = $key.Name; IconFile = $value.IconFile; IconIndex = $value.IconIndex
                    SignatureStatus = [string] $signature.Status
                    FileVersion = (Get-Item -LiteralPath $value.IconFile -ErrorAction SilentlyContinue).VersionInfo.FileVersion
                }
            }
        }
    }
}
$processes = @()
foreach ($process in Get-Process | Where-Object ProcessName -match $ProcessPattern) {
    $modules = @(); $moduleStatus = 'Read'; $moduleError = $null
    try {
        $modules = @($process.Modules | Where-Object ModuleName -eq 'ChiaKeyTsf.dll' |
            ForEach-Object { [pscustomobject]@{ Path = $_.FileName; Version = $_.FileVersionInfo.FileVersion } })
    } catch {
        $moduleStatus = 'Unavailable'; $moduleError = $_.Exception.Message
    }
    $processes += [pscustomobject]@{
        Name = $process.ProcessName; Id = $process.Id
        ModuleReadStatus = $moduleStatus; ModuleReadError = $moduleError; ChiaKeyModules = $modules
    }
}
$integrity = @(); $integrityError = $null
try {
    $events = Get-WinEvent -FilterHashtable @{
        LogName = 'Microsoft-Windows-CodeIntegrity/Operational'
        Id = 3033, 3076, 3077
        StartTime = (Get-Date).AddMinutes(-$RecentMinutes)
    } -ErrorAction Stop
    $integrity = @($events | Where-Object Message -match 'ChiaKey' |
        ForEach-Object { [pscustomobject]@{ Time = $_.TimeCreated; Id = $_.Id; Message = $_.Message } })
} catch {
    # No events and denied access are both reported; neither means the DLL loaded.
    $integrityError = $_.Exception.Message
}
$report = [ordered]@{
    CollectedAt = (Get-Date).ToString('o')
    Windows = [pscustomobject]@{ Caption = $os.Caption; Version = $os.Version; Build = $os.BuildNumber; Architecture = $os.OSArchitecture }
    Profiles = $profiles
    Processes = $processes
    CodeIntegrityEvents = $integrity
    CodeIntegrityReadError = $integrityError
    Note = 'Inspect this local report before sharing: paths may contain your Windows username. An unavailable module read is not proof of a blocked DLL.'
}
$parent = Split-Path -Parent ([IO.Path]::GetFullPath($OutputPath))
if (-not (Test-Path -LiteralPath $parent)) { throw "Output directory does not exist: $parent" }
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
Write-Output "Saved game diagnostics: $OutputPath"

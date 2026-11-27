# =============================================================================
#  Launcher: skan dysku i raport, potem decyzja.
#  Dla Windows 11 - uruchamiany z instalatora albo z pulpitu.
#
#  .\sprzataj.ps1                 -> raport + pytanie o potwierdzenie
#  .\sprzataj.ps1 -TrylkoRaport   -> nic nie rusza
#  .\sprzataj.ps1 -BezAi          -> same reguly (najszybciej)
#  .\sprzataj.ps1 -Przywroc       -> cofnij ostatnia kwarantanne
# =============================================================================

[CmdletBinding()]
param(
    [switch]$TylkoRaport,
    [switch]$BezAi,
    [switch]$Przywroc,
    [string]$Model
)

$ErrorActionPreference = "Stop"
$skrypt = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $skrypt

$argsList = @("agent.py")
if ($TylkoRaport) { $argsList += "--tylko-raport" }
if ($BezAi)        { $argsList += "--bez-ai" }
if ($Przywroc)     { $argsList += "--przywroc" }
if ($Model)        { $argsList += @("--model", $Model) }

$python = $null
foreach ($k in @("python", "py")) {
    try { & $k --version 2>&1 | Out-Null; $python = $k; break } catch { }
}

if (-not $python) {
    Write-Host "Brak Pythona. Instaluje przez winget..." -ForegroundColor Yellow
    winget install --id Python.Python.3.12 --silent --accept-package-agreements --accept-source-agreements
    $env:Path = [Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [Environment]::GetEnvironmentVariable("Path","User")
    $python = "python"
}

# UTF-8, zeby polskie znaki w raporcie nie rozsypaly konsoli
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$env:PYTHONIOENCODING = "utf-8"

& $python @argsList
$kod = $LASTEXITCODE

if ($kod -ne 0) {
    Write-Host "`nAgent zakonczyl z kodem $kod. Raporty sa w:" -ForegroundColor Yellow
    Write-Host "  $env:LOCALAPPDATA\SprzatanieDysku\raporty" -ForegroundColor Gray
}

if (-not $TylkoRaport) {
    Write-Host "`nNacisnij Enter, aby zamknac..." -ForegroundColor Gray
    Read-Host | Out-Null
}

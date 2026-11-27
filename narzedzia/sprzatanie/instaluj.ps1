# =============================================================================
#  Instalator lokalnego AI + agenta sprzatajacego dysk
#  Windows 11, PowerShell 5.1 lub 7.x
#
#  Uruchomienie (PowerShell, nie cmd):
#     Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#     .\instaluj.ps1
#
#  Co robi, po kolei:
#     1. sprawdza srodowisko (RAM, CPU, Python, miejsce),
#     2. instaluje Ollame przez winget (albo podaje link, jesli brak winget),
#     3. startuje usluge Ollama i sprawdza http://127.0.0.1:11434,
#     4. pobiera model dobrany do 16 GB RAM,
#     5. tworzy skrot do agenta na pulpicie.
#
#  Nie instaluje niczego bez pytania i nie zmienia ustawien systemu.
# =============================================================================

[CmdletBinding()]
param(
    [string]$Model = "qwen2.5:7b-instruct-q4_K_M",
    [switch]$PominModel,
    [switch]$BezPytan
)

$ErrorActionPreference = "Stop"
$skrypt = Split-Path -Parent $MyInvocation.MyCommand.Path

function Krok($t) { Write-Host "`n=== $t ===" -ForegroundColor Cyan }
function Ok($t)   { Write-Host "  [ok]  $t" -ForegroundColor Green }
function Uwaga($t){ Write-Host "  [!]   $t" -ForegroundColor Yellow }
function Blad($t) { Write-Host "  [x]   $t" -ForegroundColor Red }

function Pytaj($pytanie) {
    if ($BezPytan) { return $true }
    $o = Read-Host "$pytanie [t/N]"
    return ($o -match '^(t|tak|y|yes)$')
}

# --------------------------------------------------------------- 1. srodowisko
Krok "Sprawdzam laptopa"

# $IsWindows istnieje tylko w PowerShell 7+; w 5.1 (domyslny na Win11) jest puste,
# a tu i tak jestesmy na Windows, wiec brak zmiennej nie jest bledem.
if ($PSVersionTable.PSEdition -eq 'Core' -and $IsWindows -eq $false) {
    Blad "Ten instalator jest dla Windows. Na Linuksie uruchom: python agent.py --tylko-raport"
    exit 1
}

$cpu   = Get-CimInstance Win32_Processor | Select-Object -First 1
$ramB  = (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
$ramGB = [math]::Round($ramB / 1GB, 1)
$dysk  = Get-PSDrive -Name C
$wolne = [math]::Round($dysk.Free / 1GB, 1)

Write-Host "  CPU:    $($cpu.Name.Trim())"
Write-Host "  Rdzenie: $($cpu.NumberOfCores) / watki: $($cpu.NumberOfLogicalProcessors)"
Write-Host "  RAM:    $ramGB GB"
Write-Host "  Wolne na C: $wolne GB"

if ($ramGB -lt 8) {
    Blad "Za malo RAM ($ramGB GB). Model 7B potrzebuje ok. 6 GB wolnego."
    exit 1
}
if ($wolne -lt 12) {
    Uwaga "Malo miejsca na C: ($wolne GB). Model zajmie ok. 5 GB + 2 GB cache."
}
Ok "Sprzet wystarczy do modelu 7B na CPU (ok. 4-6 tokenow/s)."

# ------------------------------------------------------------------ 2. Python
Krok "Sprawdzam Pythona"
$py = $null
foreach ($kandydat in @("python", "py")) {
    try {
        $wersja = & $kandydat --version 2>&1
        if ($wersja -match 'Python 3\.(\d+)') {
            if ([int]$Matches[1] -ge 9) { $py = $kandydat; break }
        }
    } catch { }
}
if ($py) {
    Ok ("Python: " + (& $py --version 2>&1))
} else {
    Uwaga "Nie znalazlem Pythona 3.9+. Agent dziala tez bez niego (patrz PRZEWODNIK),"
    Uwaga "ale pelna wersja wymaga Pythona: winget install Python.Python.3.12"
}

# ------------------------------------------------------------------ 3. Ollama
Krok "Instaluje Ollame (lokalny serwer modeli)"
$ollama = Get-Command ollama -ErrorAction SilentlyContinue
if ($ollama) {
    Ok "Ollama jest juz zainstalowana: $($ollama.Source)"
} else {
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if ($winget -and (Pytaj "Zainstalowac Ollame przez winget?")) {
        winget install --id Ollama.Ollama --silent --accept-package-agreements --accept-source-agreements
        $env:Path = [Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [Environment]::GetEnvironmentVariable("Path","User")
        Ok "Ollama zainstalowana."
    } else {
        Uwaga "Pominieto instalacje. Pobierz recznie: https://ollama.com/download/windows"
        Uwaga "Potem uruchom ten skrypt ponownie."
        exit 0
    }
}

# --------------------------------------------------------------- 4. serwer AI
Krok "Uruchamiam serwer Ollama"
$dziala = $false
try {
    Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/tags" -TimeoutSec 3 | Out-Null
    $dziala = $true
    Ok "Serwer juz odpowiada na porcie 11434."
} catch {
    Start-Process -FilePath "ollama" -ArgumentList "serve" -WindowStyle Hidden
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Seconds 1
        try {
            Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/tags" -TimeoutSec 2 | Out-Null
            $dziala = $true; break
        } catch { }
    }
    if ($dziala) { Ok "Serwer wystartowal." } else { Blad "Serwer nie odpowiada. Uruchom 'ollama serve' recznie." }
}

# ------------------------------------------------------------------- 5. model
Krok "Model lokalny"
if ($PominModel) {
    Uwaga "Pominieto pobieranie modelu (-PominModel)."
} elseif ($dziala) {
    $dostepne = & ollama list 2>&1 | Out-String
    if ($dostepne -match [regex]::Escape(($Model -split ':')[0])) {
        Ok "Model $Model juz jest pobrany."
    } else {
        Write-Host "  Pobieram $Model (~4.7 GB, zalezy od lacza: 5-30 min)"
        if (Pytaj "Pobrac teraz?") {
            & ollama pull $Model
            if ($LASTEXITCODE -eq 0) { Ok "Model pobrany." } else { Blad "Pobieranie nie powiodlo sie." }
        } else {
            Uwaga "Mozesz pobrac pozniej: ollama pull $Model"
        }
    }

    # Test: czy model odpowiada po polsku
    if ($dostepne -match [regex]::Escape(($Model -split ':')[0])) {
        Write-Host "  Testuje model (moze potrwac 30-60 s na CPU)..."
        $test = '{"model":"' + $Model + '","prompt":"Odpowiedz jednym slowem: dziala?","stream":false}'
        try {
            $odp = Invoke-RestMethod -Uri "http://127.0.0.1:11434/api/generate" `
                   -Method Post -Body $test -ContentType "application/json" -TimeoutSec 180
            Ok ("Model odpowiada: " + $odp.response.Trim())
        } catch {
            Uwaga "Model nie odpowiedzial w 180 s. Na 4750U pierwsze uruchomienie"
            Uwaga "laduje model do RAM - drugie jest juz szybkie."
        }
    }
}

# ------------------------------------------------------------- 6. skrot na pulpit
Krok "Skrot na pulpicie"
$pulpit = [Environment]::GetFolderPath("Desktop")
$skrotPlik = Join-Path $pulpit "Sprzatanie dysku (raport).lnk"
$ws = New-Object -ComObject WScript.Shell
$skrot = $ws.CreateShortcut($skrotPlik)
$skrot.TargetPath = "powershell.exe"
$skrot.Arguments = "-NoExit -ExecutionPolicy Bypass -File `"$(Join-Path $skrypt 'sprzataj.ps1')`" -TylkoRaport"
$skrot.WorkingDirectory = $skrypt
$skrot.Description = "Skan dysku i raport - nic nie usuwa"
$skrot.Save()
Ok "Skrot: $skrotPlik"

# ---------------------------------------------------------------------- koniec
Krok "Gotowe"
Write-Host @"
  Najpierw bezpiecznie, tylko raport:
     cd "$skrypt"
     python agent.py --tylko-raport

  Potem sprzatanie z potwierdzeniem:
     python agent.py --bez-ai          # same reguly, 10 s
     python agent.py                   # reguly + lokalny model

  Cofniecie ostatniej kwarantanny:
     python agent.py --przywroc

  Model: $Model
  Serwer: http://127.0.0.1:11434 (dziala tylko lokalnie, nie wychodzi do internetu)
"@ -ForegroundColor Gray

<#
.SYNOPSIS
  Znajduje port COM plytki ESP32-C3 (Windows / PowerShell).

.DESCRIPTION
  Powod powstania: PlatformIO bierze "pierwszy lepszy" port COM ("Auto-detected:
  COM5"), a na biurku czesto wisi wiecej niz jedna plytka — u nas panel LCD jest
  ESP32 (env "esp32dev"), wiec esptool konczy sie bledem:

      A fatal error occurred: This chip is ESP32 not ESP32-C3. Wrong --chip argument?

  Ten skrypt:
    1. wypisuje WSZYSTKIE porty COM z opisem sterownika (CH340/CH343/CP210x/USB JTAG),
    2. z -ChipId probuje na kazdym porcie odczytac uklad (esptool z PlatformIO)
       i mowi, ktory port to ESP32-C3, a ktory to zwykly ESP32 (panel).

.PRZYKLADY
  powershell -ExecutionPolicy Bypass -File narzedzia\znajdz_port.ps1
  powershell -ExecutionPolicy Bypass -File narzedzia\znajdz_port.ps1 -ChipId
  powershell -ExecutionPolicy Bypass -File narzedzia\znajdz_port.ps1 -ChipId -TimeoutSec 30

.UWAGA
  Przed uruchomieniem zamknij monitor szeregowy (pio device monitor) — zajety port
  da "could not open port".
#>
param(
  [switch]$ChipId,
  [int]$TimeoutSec = 20
)

$ErrorActionPreference = 'Continue'

function Get-PortyCom {
  Get-CimInstance Win32_PnPEntity |
    Where-Object { $_.Name -match '\(COM\d+\)' } |
    ForEach-Object {
      $m = [regex]::Match($_.Name, '\((COM\d+)\)')
      [pscustomobject]@{
        Port     = $m.Groups[1].Value
        Opis     = ($_.Name -replace '\s*\(COM\d+\)', '').Trim()
        DeviceID = $_.DeviceID
      }
    } | Sort-Object { [int]($_.Port -replace 'COM', '') }
}

Write-Host ""
Write-Host "Porty COM widziane przez Windows:" -ForegroundColor Cyan
Write-Host ("-" * 78)

$porty = @(Get-PortyCom)
if ($porty.Count -eq 0) {
  Write-Host "  (brak) — zadne urzadzenie nie wystawia portu COM." -ForegroundColor Yellow
  Write-Host ""
  Write-Host "Najczestsze przyczyny:" -ForegroundColor Yellow
  Write-Host "  * kabel USB tylko do ladowania (bez linii danych),"
  Write-Host "  * brak sterownika CH340/CH343 (C3 SuperMini) albo CP210x (DevKitM),"
  Write-Host "  * C3 z natywnym USB: przytrzymaj BOOT, nacisnij i pusc RESET, pusc BOOT."
  exit 1
}

foreach ($p in $porty) {
  Write-Host ("  {0,-6} {1,-34} {2}" -f $p.Port, $p.Opis, $p.DeviceID)
}
Write-Host ("-" * 78)
Write-Host ""

if (-not $ChipId) {
  Write-Host "Wskazowka: dodaj -ChipId, zeby sprawdzic, ktory port to ESP32-C3." -ForegroundColor DarkGray
  Write-Host "Nastepnie wpisz port w centrala-testowa-c3\platformio.ini:" -ForegroundColor DarkGray
  Write-Host "    upload_port = COM7" -ForegroundColor DarkGray
  Write-Host "    monitor_port = COM7" -ForegroundColor DarkGray
  exit 0
}

# --- sciezki narzedzi z PlatformIO ---
$pioHome = Join-Path $env:USERPROFILE ".platformio"
$python  = Join-Path $pioHome "penv\Scripts\python.exe"
$esptool = Join-Path $pioHome "packages\tool-esptoolpy\esptool.py"

if (-not (Test-Path $python)) {
  Write-Host "Nie znalazlem Pythona PlatformIO: $python" -ForegroundColor Red
  Write-Host "Uruchom najpierw 'pio run' (zainstaluje narzedzia), potem sprobuj ponownie." -ForegroundColor Yellow
  exit 1
}
if (-not (Test-Path $esptool)) {
  Write-Host "Nie znalazlem esptool: $esptool" -ForegroundColor Red
  Write-Host "Uruchom 'pio run' w projekcie (PlatformIO pobierze tool-esptoolpy)." -ForegroundColor Yellow
  exit 1
}

$outPlik = Join-Path $env:TEMP "esptool_chipid_out.txt"
$errPlik = Join-Path $env:TEMP "esptool_chipid_err.txt"

$c3Ports     = @()
$esp32Ports  = @()
$innePorts   = @()

foreach ($p in $porty) {
  Write-Host ("Sprawdzam {0} ({1}) ... " -f $p.Port, $p.Opis) -NoNewline
  Remove-Item $outPlik, $errPlik -ErrorAction SilentlyContinue

  $proc = Start-Process -FilePath $python `
            -ArgumentList @($esptool, '--port', $p.Port, 'chip_id') `
            -NoNewWindow -PassThru `
            -RedirectStandardOutput $outPlik -RedirectStandardError $errPlik

  $zakonczyl = $proc.WaitForExit($TimeoutSec * 1000)
  if (-not $zakonczyl) {
    try { $proc.Kill() } catch {}
    Write-Host "TIMEOUT (port zajety / brak odpowiedzi ukladu)" -ForegroundColor Yellow
    $innePorts += $p.Port
    continue
  }

  $tekst = ""
  if (Test-Path $outPlik) { $tekst += (Get-Content $outPlik -Raw) }
  if (Test-Path $errPlik) { $tekst += (Get-Content $errPlik -Raw) }
  if (-not $tekst) { $tekst = "" }

  if ($tekst -match 'ESP32-C3|ESP32C3') {
    Write-Host "ESP32-C3  <-- TO TA PLYTKA" -ForegroundColor Green
    $c3Ports += $p.Port
  } elseif ($tekst -match 'ESP32-S3|ESP32S3') {
    Write-Host "ESP32-S3" -ForegroundColor Cyan
    $innePorts += $p.Port
  } elseif ($tekst -match 'ESP32-S2|ESP32S2') {
    Write-Host "ESP32-S2" -ForegroundColor Cyan
    $innePorts += $p.Port
  } elseif ($tekst -match 'ESP32-C6|ESP32C6') {
    Write-Host "ESP32-C6" -ForegroundColor Cyan
    $innePorts += $p.Port
  } elseif ($tekst -match 'ESP32') {
    Write-Host "ESP32 (klasyczny — to NIE jest C3; pewnie panel LCD)" -ForegroundColor Yellow
    $esp32Ports += $p.Port
  } else {
    $powod = "brak odpowiedzi"
    if ($tekst -match 'could not open port|PermissionError|in use') { $powod = "port zajety (zamknij monitor)" }
    elseif ($tekst -match 'Failed to connect|no serial data') { $powod = "uklad nie odpowiada (tryb download? BOOT+RESET)" }
    Write-Host "nieznany ($powod)" -ForegroundColor DarkYellow
    $innePorts += $p.Port
  }
}

Write-Host ""
Write-Host ("=" * 78)
if ($c3Ports.Count -eq 1) {
  Write-Host "WYNIK: ESP32-C3 jest na $($c3Ports[0])." -ForegroundColor Green
  Write-Host ""
  Write-Host "Wpisz do centrala-testowa-c3\platformio.ini:" -ForegroundColor Green
  Write-Host "    upload_port = $($c3Ports[0])"
  Write-Host "    monitor_port = $($c3Ports[0])"
  Write-Host ""
  Write-Host "albo wgraj od razu:" -ForegroundColor Green
  Write-Host "    pio run -t upload --upload-port $($c3Ports[0])"
} elseif ($c3Ports.Count -gt 1) {
  Write-Host "WYNIK: wiecej niz jeden ESP32-C3 podlaczony: $($c3Ports -join ', ')" -ForegroundColor Yellow
} else {
  Write-Host "WYNIK: nie znalazlem ESP32-C3 na zadnym porcie." -ForegroundColor Red
  if ($esp32Ports.Count -gt 0) {
    Write-Host "  ESP32 (klasyczny, np. panel LCD) jest na: $($esp32Ports -join ', ')" -ForegroundColor Yellow
    Write-Host "  Odlacz te plytke i podlacz sam ESP32-C3, potem powtorz." -ForegroundColor Yellow
  }
  Write-Host "  Sprawdz kabel USB (dane, nie tylko ladowanie) i sterownik CH340/CH343/CP210x."
  Write-Host "  C3 z natywnym USB: BOOT + RESET, zeby wejsc w tryb downloadu."
}

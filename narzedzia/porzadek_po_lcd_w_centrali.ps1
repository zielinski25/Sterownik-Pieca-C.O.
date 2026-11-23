# ============================================================================
# porzadek_po_lcd_w_centrali.ps1 - 2026-11-26
# Sytuacja: paczka panelu LCD (LCD-Centrali_v1.5.12_*.zip) zostala rozpakowana
# do katalogu projektu CENTRALI (Documents\PlatformIO\sterownik-co-esp32).
# Panel ma te same nazwy co centrala dla 4 plikow, ktore zostaly NADPISANE:
#     platformio.ini, partitions.csv, README.md, .gitignore
# (+ include\README i lib\README - szablony PlatformIO, bez znaczenia).
# Reszta plikow panelu lezy obok plikow centrali (nic nie nadpisala), ale
# src\main.cpp panelu NIE MOZE zostac - drugie setup()/loop() = blad kompilacji.
# Plik src\secrets.h centrali NIE byl ruszony (paczka ma tylko secrets.h.example).
#
#   .\porzadek_po_lcd_w_centrali.ps1            tylko RAPORT (nic nie zmienia)
#   .\porzadek_po_lcd_w_centrali.ps1 -Napraw    kopia zapasowa ZIP + usuniecie
#                                               plikow panelu + odzyskanie
#                                               partitions.csv z .pio
# Uruchamiac z katalogu projektu centrali albo podac -Projekt sciezka.
# Gdy PowerShell odmawia: powershell -ExecutionPolicy Bypass -File .\porzadek_po_lcd_w_centrali.ps1
# ============================================================================
param(
  [string]$Projekt = (Get-Location).Path,
  [switch]$Napraw
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $Projekt
Write-Host "Projekt: $Projekt" -ForegroundColor Cyan

# --- pliki panelu, ktore w centrali nie maja czego szukac (lista z ZIP-a) ----
$PlikiPanelu = @(
  'ekran_aktualizacja.png', 'ekran_alarm_bez_danych.png', 'ekran_centrala_milczy.png',
  'ekran_gola_plytka.png', 'ekran_wykres_brak_danych.png', 'ekran_wykres_dzien.png',
  'ekran_wykres_tydzien_przerwa.png',
  'include\lv_conf.h',
  'narzedzia_pio\ota_haslo.py',
  'src\czcionki\CZYTAJ.txt', 'src\czcionki\czcionki_pl.h',
  'src\czcionki\lv_font_pl_14.c', 'src\czcionki\lv_font_pl_20.c', 'src\czcionki\lv_font_pl_28.c',
  'src\dashboard_lvgl_poc.cpp', 'src\dashboard_lvgl_poc.h', 'src\diagnostyka_panelu.h',
  'src\ikony.h', 'src\ikony_lvgl.h', 'src\lvgl_port.cpp', 'src\lvgl_port.h',
  'src\secrets.h.example', 'src\siec_nieblokujaca_LCD.h',
  'docs\schemat-zasilania\README.md', 'docs\schemat-zasilania\brzeczyk.html',
  'docs\schemat-zasilania\dzielnik.html', 'docs\schemat-zasilania\index.html',
  'docs\schemat-zasilania\symulacja.html'
)
# src\main.cpp - tylko jesli to NA PEWNO plik panelu
$MainPanelu = $false
if (Test-Path 'src\main.cpp') {
  $MainPanelu = (Select-String -LiteralPath 'src\main.cpp' -Pattern 'FIRMWARE_VERSION_PANEL' -Quiet)
}
# pliki NADPISANE - rozpoznanie po tresci panelu
$Nadpisane = @{}
if (Test-Path 'platformio.ini') { $Nadpisane['platformio.ini'] = (Select-String -LiteralPath 'platformio.ini' -Pattern 'Wyswietlacz CO|env:esp32dev\]' -Quiet) }
if (Test-Path 'partitions.csv') { $Nadpisane['partitions.csv'] = (Select-String -LiteralPath 'partitions.csv' -Pattern '0x1F0000' -Quiet) }
if (Test-Path 'README.md')      { $Nadpisane['README.md']      = (Select-String -LiteralPath 'README.md' -Pattern 'Panel LCD \(wyświetlacz CO\)' -Quiet) }
if (Test-Path '.gitignore')     { $Nadpisane['.gitignore']     = (Select-String -LiteralPath '.gitignore' -Pattern 'Hasla panelu' -Quiet) }

Write-Host ""
Write-Host "=== 1. Pliki centrali NADPISANE przez panel ===" -ForegroundColor Yellow
foreach ($k in $Nadpisane.Keys) {
  if ($Nadpisane[$k]) { Write-Host "  [PANEL]    $k  - to jest plik panelu, centrali juz tu nie ma" -ForegroundColor Red }
  else                { Write-Host "  [ok]       $k  - wyglada na plik centrali" -ForegroundColor Green }
}

Write-Host ""
Write-Host "=== 2. Pliki panelu lezace obok plikow centrali ===" -ForegroundColor Yellow
$DoUsuniecia = @()
foreach ($p in $PlikiPanelu) { if (Test-Path -LiteralPath $p) { $DoUsuniecia += $p; Write-Host "  $p" } }
if ($MainPanelu) { $DoUsuniecia += 'src\main.cpp'; Write-Host "  src\main.cpp   (zawiera FIRMWARE_VERSION_PANEL = plik panelu)" -ForegroundColor Red }
elseif (Test-Path 'src\main.cpp') { Write-Host "  src\main.cpp   - NIE jest plikiem panelu, zostaje" -ForegroundColor Green }
Write-Host "  razem: $($DoUsuniecia.Count)"

Write-Host ""
Write-Host "=== 3. Co zostaje w src\ (pliki centrali) ===" -ForegroundColor Yellow
Get-ChildItem src -File | Where-Object { ($DoUsuniecia -notcontains ('src\' + $_.Name)) } | ForEach-Object { Write-Host ("  {0,-40} {1,10} B  {2}" -f $_.Name, $_.Length, $_.LastWriteTime) }
if (Test-Path 'src\secrets.h') { Write-Host "  src\secrets.h jest (hasla centrali nie ruszone)" -ForegroundColor Green } else { Write-Host "  UWAGA: brak src\secrets.h!" -ForegroundColor Red }
if (Test-Path 'src\main_centrala.cpp') {
  $w = Select-String -LiteralPath 'src\main_centrala.cpp' -Pattern 'static const char\* FIRMWARE_VERSION = "(v[0-9.]+)"' | Select-Object -First 1
  if ($w) { Write-Host "  src\main_centrala.cpp: $($w.Matches[0].Groups[1].Value)" -ForegroundColor Green }
}

Write-Host ""
Write-Host "=== 4. Skad odzyskac nadpisane pliki ===" -ForegroundColor Yellow
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git -and (Test-Path '.git')) {
  Write-Host "  Katalog jest repozytorium git - najprosciej:" -ForegroundColor Green
  Write-Host "     git status --short"
  Write-Host "     git checkout -- platformio.ini partitions.csv README.md .gitignore"
} else { Write-Host "  .git: brak (nie ma skad zrobic checkout)" }
$part = Get-ChildItem '.pio\build\*\partitions.bin' -ErrorAction SilentlyContinue
if ($part) {
  foreach ($pb in $part) { Write-Host "  partitions.bin z ostatniej kompilacji: $($pb.FullName)  ($($pb.LastWriteTime))" -ForegroundColor Green }
  Write-Host "  -> -Napraw odtworzy z niego partitions.csv (gen_esp32part.py z pakietu Arduino-ESP32)"
} else { Write-Host "  .pio\build\*\partitions.bin: brak" }
$cpp = '.vscode\c_cpp_properties.json'
if (Test-Path $cpp) { Write-Host "  $cpp jest - ma build_flags/defines i sciezki z DAWNEGO platformio.ini (do odtworzenia recznie)" -ForegroundColor Green }
Write-Host "  Windows: prawy klawisz na folderze sterownik-co-esp32 -> Wlasciwosci -> Poprzednie wersje"
Write-Host "  (Historia plikow / OneDrive, jesli Dokumenty sa synchronizowane: onedrive.com -> Historia wersji)"
$sdk = Get-ChildItem 'sdkconfig.*' -ErrorAction SilentlyContinue | ForEach-Object { $_.Name }
if ($sdk) { Write-Host "  sdkconfig.*: $($sdk -join ', ')  (nazwy srodowisk z dawnego platformio.ini)" }
$boards = Get-ChildItem 'boards\*.json' -ErrorAction SilentlyContinue | ForEach-Object { $_.Name }
if ($boards) { Write-Host "  boards\: $($boards -join ', ')" }

if (-not $Napraw) {
  Write-Host ""
  Write-Host "To byl tylko raport. Zeby posprzatac: .\porzadek_po_lcd_w_centrali.ps1 -Napraw" -ForegroundColor Cyan
  exit 0
}

# ============================ NAPRAWA =======================================
Write-Host ""
Write-Host "=== NAPRAWA ===" -ForegroundColor Cyan
$stempel = Get-Date -Format 'yyyyMMdd-HHmmss'
$kopia = Join-Path (Split-Path -Parent $Projekt) ("sterownik-co-esp32-kopia-przed-porzadkiem-$stempel.zip")
Write-Host "1. Kopia zapasowa calego katalogu (bez .pio): $kopia"
$tmp = Join-Path $env:TEMP "kopia-centrali-$stempel"
New-Item -ItemType Directory -Path $tmp | Out-Null
Get-ChildItem -Force | Where-Object { $_.Name -ne '.pio' } | Copy-Item -Destination $tmp -Recurse -Force
Compress-Archive -Path (Join-Path $tmp '*') -DestinationPath $kopia -Force
Remove-Item -LiteralPath $tmp -Recurse -Force
Write-Host "   zapisano $([math]::Round((Get-Item $kopia).Length/1MB,1)) MB" -ForegroundColor Green

Write-Host "2. Usuwam pliki panelu ($($DoUsuniecia.Count))"
foreach ($p in $DoUsuniecia) { Remove-Item -LiteralPath $p -Force; Write-Host "   - $p" }
foreach ($d in @('src\czcionki', 'narzedzia_pio', 'docs\schemat-zasilania', 'docs')) {
  if ((Test-Path -LiteralPath $d) -and -not (Get-ChildItem -LiteralPath $d -Force | Select-Object -First 1)) { Remove-Item -LiteralPath $d -Force; Write-Host "   - $d\ (pusty katalog)" }
}

Write-Host "3. Nadpisane pliki panelu -> zmieniam nazwe na *.PANEL (zeby nie udawaly plikow centrali)"
foreach ($k in $Nadpisane.Keys) {
  if ($Nadpisane[$k]) { Move-Item -LiteralPath $k -Destination "$k.PANEL" -Force; Write-Host "   $k -> $k.PANEL" }
}

Write-Host "4. partitions.csv z .pio\build\<env>\partitions.bin"
$gen = Get-ChildItem "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32*\tools\gen_esp32part.py" -ErrorAction SilentlyContinue | Select-Object -First 1
$py  = Get-ChildItem "$env:USERPROFILE\.platformio\python3\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $py) { $py = Get-Command python -ErrorAction SilentlyContinue }
$pb = Get-ChildItem '.pio\build\*\partitions.bin' -ErrorAction SilentlyContinue | Where-Object { $_.DirectoryName -notmatch 'esp32dev' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($gen -and $py -and $pb -and -not (Test-Path 'partitions.csv')) {
  & $py.FullName $gen.FullName $pb.FullName 'partitions.csv'
  Write-Host "   odtworzono partitions.csv z $($pb.FullName):" -ForegroundColor Green
  Get-Content 'partitions.csv' | ForEach-Object { Write-Host "     $_" }
  Write-Host "   (sprawdz: app0/app1 po 0x300000, spiffs/littlefs 0x9E0000 - takie ma centrala N16R8)"
} elseif (Test-Path 'partitions.csv') {
  Write-Host "   partitions.csv juz jest (plik centrali) - nie ruszam"
} else {
  Write-Host "   nie udalo sie: gen_esp32part.py=$([bool]$gen) python=$([bool]$py) partitions.bin=$([bool]$pb)" -ForegroundColor Red
  Write-Host "   Wklej recznie partitions.csv centrali (tresc w odpowiedzi agenta)."
}

Write-Host ""
Write-Host "GOTOWE. Zostalo do zrobienia RECZNIE:" -ForegroundColor Yellow
Write-Host "  - platformio.ini centrali (env esp32-s3-n16r8) - z git / Poprzednich wersji / OneDrive,"
Write-Host "    albo odtworzyc wg .vscode\c_cpp_properties.json (przyslij ten plik + boards\*.json)."
Write-Host "  - README.md i .gitignore centrali (nieistotne dla kompilacji)."
Write-Host "  - potem: pio run -e esp32-s3-n16r8  (ma sie skompilowac bez bledu o podwojnym setup())"

# =====================================================================
#  SEKRETY.ps1  -  wpisuje 5 brakujacych wartosci do src\secrets.h
#  Uruchom na Windows PowerShell 5.1 (ten sam, ktory masz otwarty).
#  Plik jest czysto ASCII (bez polskich znakow), zeby PS 5.1 go nie
#  zepsul przy odczycie.
#
#  Skrypt:
#    1. robi kopie starego secrets.h (z data w nazwie),
#    2. pyta o kazda wartosc (nic nie jest zapisane w samym skrypcie),
#    3. zostawia istniejacy SECRET_HISTORIA_TOKEN (albo losuje nowy),
#    4. zapisuje secrets.h w ASCII,
#    5. pokazuje podsumowanie BEZ wypisywania hasel.
# =====================================================================

$Projekt = "C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32"
$Src     = Join-Path $Projekt "src"
$Plik    = Join-Path $Src "secrets.h"

if (-not (Test-Path $Src)) {
  Write-Host "Nie ma folderu: $Src" -ForegroundColor Red
  return
}

# --- kopia starego pliku -------------------------------------------------
$Historia = ""
if (Test-Path $Plik) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  Copy-Item $Plik "$Plik.bak-$stamp" -Force
  Write-Host "Kopia starego pliku: secrets.h.bak-$stamp" -ForegroundColor DarkGray

  $m = Select-String -Path $Plik -Pattern 'SECRET_HISTORIA_TOKEN\s+"([^"]*)"' | Select-Object -First 1
  if ($m) { $Historia = $m.Matches[0].Groups[1].Value }
}
if (-not $Historia) {
  $Historia = -join (1..32 | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
}

function BezCudzyslowu($s) { return ($s -replace '"', '').Trim() }

# --- pytania -------------------------------------------------------------
Write-Host ""
Write-Host "1/5  SSID sieci, ktora rozglasza piec (hotspot awaryjny)." -ForegroundColor Cyan
$ap = BezCudzyslowu (Read-Host "     SSID [PiecCO_AP]")
if (-not $ap) { $ap = "PiecCO_AP" }

Write-Host ""
Write-Host "2/5  Haslo do tej sieci (min. 8 znakow - wymog WPA2)." -ForegroundColor Cyan
do { $apPass = BezCudzyslowu (Read-Host "     haslo") } while ($apPass.Length -lt 8)

Write-Host ""
Write-Host "3/5  Haslo OTA - tym haslem wgrywa sie firmware bez kabla (VSCode / przegladarka)." -ForegroundColor Cyan
do { $ota = BezCudzyslowu (Read-Host "     haslo OTA (min. 8 znakow)") } while ($ota.Length -lt 8)

Write-Host ""
Write-Host "4/5  Haslo konta Firebase piec_co_boot@akwarium.local." -ForegroundColor Cyan
Write-Host "     WAZNE: dopoki w NVS sa juz zapisane dane logowania, to haslo NIE jest uzywane." -ForegroundColor DarkGray
Write-Host "     Uzywane jest tylko przy pustej pamieci NVS. Mozesz wpisac dowolne >= 8 znakow," -ForegroundColor DarkGray
Write-Host "     a potem naprawic przez Telegram: /fbprov|KLUCZ|piec_co_boot@akwarium.local|HASLO" -ForegroundColor DarkGray
do { $fb = BezCudzyslowu (Read-Host "     haslo Firebase (min. 8 znakow)") } while ($fb.Length -lt 8)

Write-Host ""
Write-Host "5/5  Token komend - TEN SAM, ktory ma aplikacja i panel." -ForegroundColor Cyan
Write-Host "     Gdzie go odczytac: aplikacja -> Ustawienia -> token komend," -ForegroundColor DarkGray
Write-Host "     albo panel pieca -> Ustawienia -> Polaczenie z baza Firebase -> pole tokenu." -ForegroundColor DarkGray
Write-Host "     Pusty token = kazdy w sieci moze sterowac piecem, dlatego nie przyjmuje pustego." -ForegroundColor DarkGray
do { $tok = BezCudzyslowu (Read-Host "     token komend (min. 8 znakow)") } while ($tok.Length -lt 8)

# --- zapis ---------------------------------------------------------------
$linie = @(
  "#pragma once",
  "// secrets.h - wygenerowany przez SEKRETY.ps1  $(Get-Date -Format 'yyyy-MM-dd HH:mm')",
  "// Ten plik jest w .gitignore. Nie wysylaj go nigdzie.",
  "",
  "#define SECRET_WIFI_AP_SSID     `"$ap`"",
  "#define SECRET_WIFI_AP_PASSWORD `"$apPass`"",
  "#define SECRET_OTA_HASLO        `"$ota`"",
  "#define SECRET_FIREBASE_PASS    `"$fb`"",
  "#define SECRET_CMD_TOKEN        `"$tok`"",
  "#define SECRET_HISTORIA_TOKEN   `"$Historia`""
)
Set-Content -Path $Plik -Value $linie -Encoding ASCII

Write-Host ""
Write-Host "Zapisane: $Plik" -ForegroundColor Green
Write-Host ("  SSID           : {0} ({1} znakow)" -f $ap, $ap.Length)
Write-Host ("  haslo AP       : {0} znakow" -f $apPass.Length)
Write-Host ("  haslo OTA      : {0} znakow" -f $ota.Length)
Write-Host ("  haslo Firebase : {0} znakow" -f $fb.Length)
Write-Host ("  token komend   : {0} znakow" -f $tok.Length)
Write-Host ("  token historii : {0} znakow" -f $Historia.Length)
Write-Host ""
Write-Host "Teraz uruchom SPRAWDZ.ps1 - sekcja secrets.h powinna byc cala OK." -ForegroundColor Yellow

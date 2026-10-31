# =====================================================================
#  TEST-HISTORII.ps1  -  sprawdza pobranie historii CSV z pieca,
#  bez telefonu, z tego komputera (PowerShell 5.1, plik czysto ASCII).
#
#  Robi DOKLADNIE to samo, co robi przegladarka po kliknieciu przycisku,
#  tylko token jest w naglowku Authorization zamiast w bilecie:
#     GET http://<piec>/history        -> historia-krotka.csv
#     GET http://<piec>/history/long   -> historia-dluga.csv
#
#  Pliki laduja w folderze Pobrane. Skrypt wypisuje kod HTTP, rozmiar,
#  liczbe wierszy i pierwszy wiersz (naglowek kolumn).
# =====================================================================

$Src = "C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32\src"

# --- token z secrets.h (ten sam, ktory ma firmware) ---------------------
$token = ""
$m = Select-String -Path "$Src\secrets.h" -Pattern 'SECRET_HISTORIA_TOKEN\s+"([^"]*)"' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($m) { $token = $m.Matches[0].Groups[1].Value }
if (-not $token) { $token = (Read-Host "Token historii (SECRET_HISTORIA_TOKEN)").Trim() }
if (-not $token) { Write-Host "Bez tokenu nie ma czego sprawdzac." -ForegroundColor Red; return }
Write-Host ("Token: {0} znakow" -f $token.Length) -ForegroundColor DarkGray

# --- adres pieca --------------------------------------------------------
Write-Host ""
Write-Host "Adres pieca: wpisz IP, ktore widzisz w panelu (np. 192.168.1.50)," -ForegroundColor Cyan
Write-Host "albo nacisnij Enter, jesli dziala sterownikco.local." -ForegroundColor Cyan
$adres = (Read-Host "Adres").Trim()
if (-not $adres) { $adres = "sterownikco.local" }
$adres = $adres -replace "^https?://", ""
$adres = $adres.TrimEnd("/")

function PobierzHistorie([string]$Sciezka, [string]$Nazwa) {
  $out = Join-Path "$env:USERPROFILE\Downloads" $Nazwa
  if (Test-Path $out) { Remove-Item $out -Force }
  $kod = 0
  $disp = ""
  $blad = ""
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  try {
    $r = Invoke-WebRequest -Uri ("http://" + $adres + $Sciezka) `
          -Headers @{ Authorization = ("Bearer " + $token) } `
          -OutFile $out -UseBasicParsing -TimeoutSec 180 -PassThru -ErrorAction Stop
    $kod = [int]$r.StatusCode
    $disp = [string]$r.Headers['Content-Disposition']
  } catch {
    $blad = $_.Exception.Message
    if ($_.Exception.Response -ne $null) { $kod = [int]$_.Exception.Response.StatusCode }
  }
  $sw.Stop()

  $bajty = 0
  $linii = 0
  $pierwsza = ""
  if (Test-Path $out) {
    $bajty = (Get-Item $out).Length
    if ($bajty -gt 0) {
      $pierwsza = [string](Get-Content $out -TotalCount 1 -Encoding UTF8)
      $linii = (Get-Content $out -Encoding UTF8 | Measure-Object -Line).Lines
    }
  }

  $ok = ($kod -eq 200) -and ($bajty -gt 0)

  Write-Host ""
  Write-Host ("--- {0}  ({1}) ---" -f $Nazwa, $Sciezka) -ForegroundColor Cyan
  if ($kod -eq 0) {
    Write-Host ("  BLAD POLACZENIA: {0}" -f $blad) -ForegroundColor Red
    Write-Host "  Piec nieosiagalny z tego komputera. Sprawdz IP i to, czy jestes w tej samej sieci." -ForegroundColor DarkGray
  } else {
    $kolor = "Red"
    if ($ok) { $kolor = "Green" }
    Write-Host ("  HTTP      : {0}" -f $kod) -ForegroundColor $kolor
    Write-Host ("  czas      : {0:N1} s" -f $sw.Elapsed.TotalSeconds)
    Write-Host ("  rozmiar   : {0} B" -f $bajty)
    Write-Host ("  wierszy   : {0}" -f $linii)
    if ($disp) { Write-Host ("  naglowek  : {0}" -f $disp) -ForegroundColor DarkGray }
    if ($pierwsza) {
      if ($pierwsza.Length -gt 160) { $pierwsza = $pierwsza.Substring(0, 160) + "..." }
      Write-Host ("  1. wiersz : {0}" -f $pierwsza) -ForegroundColor DarkGray
    }
    if ($ok) {
      Write-Host ("  PLIK: {0}" -f $out) -ForegroundColor Green
    } else {
      Write-Host "  Plik nie powstal albo jest pusty." -ForegroundColor Red
    }

    switch ($kod) {
      401 { Write-Host "  -> token sie NIE zgadza: w secrets.h inny niz w firmware." -ForegroundColor Yellow }
      404 { Write-Host "  -> firmware nie ma jeszcze zadnej historii do pobrania." -ForegroundColor Yellow }
      413 { Write-Host "  -> plik wiekszy niz limit dokumentu." -ForegroundColor Yellow }
      503 { Write-Host "  -> pobieranie wylaczone (pusty token), trwa inne pobranie albo magazyn niegotowy." -ForegroundColor Yellow }
      500 { Write-Host "  -> blad dostepu do pliku historii (LittleFS)." -ForegroundColor Yellow }
    }
  }
  return $ok
}

Write-Host ""
Write-Host ("Piec: http://{0}" -f $adres) -ForegroundColor DarkGray

$a = PobierzHistorie "/history" "historia-krotka.csv"
Start-Sleep -Milliseconds 800
$b = PobierzHistorie "/history/long" "historia-dluga.csv"

Write-Host ""
if ($a -and $b) {
  Write-Host "WYNIK: OBA pliki pobrane. Funkcja dziala." -ForegroundColor Green
  Write-Host "Teraz to samo na telefonie: panel -> HISTORIA CSV -> Krotka / Dluga." -ForegroundColor Yellow
} elseif ($a -or $b) {
  Write-Host "WYNIK: jeden plik OK, drugi nie - patrz kod HTTP wyzej." -ForegroundColor Yellow
} else {
  Write-Host "WYNIK: zadnego pliku. Patrz kody HTTP wyzej." -ForegroundColor Red
}

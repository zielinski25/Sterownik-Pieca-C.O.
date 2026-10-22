════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.26  →  v0.31.27
 DZIŚ = dzisiejsze próbki, JEDEN model prognozy, sufit 45 °C
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.26. Nadpisuje 4 pliki. Wersja: 3033 / "v0.31.27".

Powód: zrzut z 22.10 pokazał „DZIŚ +93.1 °C", gdy model na dziś dawał
+9.1 °C (289 W/m² → 2.17 kWh/m² × 4,2). Ten sam 93.1 był w „Zysku brutto
słońca". Dodatkowo „Prognoza zysku" pokazywała +0.0 °C obok +19.0 °C
na karcie JUTRO — dwa różne modele w jednym ekranie.


────────────────────────────────────────────────────────────────────────────
 1. CO BYŁO ŹLE
────────────────────────────────────────────────────────────────────────────

a) KARTA „DZIŚ" BRAŁA SUMĘ Z CAŁEGO BUFORA
     estGainC = if (today && accumulatedGrossGain > 0)
                    maxOf(accumulatedGrossGain, modelGain)   // ← zawsze wygrywała
                else modelGain
   accumulatedGrossGain to suma przyrostów z do 500 próbek, sięgająca kilka
   dni wstecz. Na zrzucie: model 9.1 °C, wyświetlone 93.1 °C.

b) „PROGNOZA ZYSKU" LICZONA INNYM WZOREM
   model godzinowy: (100 − zachmurzenie) × 8.5  →  dla pochmurnego dnia 0.0
   karty 7 dni:     pik W/m² × 7,5 / 1000 × 4,2 →  dziś 9.1
   Dwie różne liczby na jednym ekranie, obie podpisane „prognoza".

c) BRAK SUFITU
   Model mnożył GHI bez ograniczenia. Przy dużym nasłonecznieniu mógł
   pokazać +93 °C dla zbiornika 100 l, który w jednej dobie zagotować
   się nie może.


────────────────────────────────────────────────────────────────────────────
 2. SKĄD SIĘ BIERZE STAŁA 4,2 °C NA 1 kWh/m²
────────────────────────────────────────────────────────────────────────────

Dane katalogowe kolektorów 10 rur 58/1800:

     SOLEKO APS 10         apertura 0,90 m²   sprawność opt. 68,7%
     Projprzem PE10-58     apertura 1,00 m²   sprawność maks. 81,5%
     IB-Sol 10             apertura 1,12 m²   sprawność opt. 65,8%
     Hewalex KSR10         apertura 1,01 m²   sprawność opt. 78%

Uzysk roczny kolektora próżniowego: 450–600 kWh/m².

Rachunek dla tej instalacji (zbiornik 100 l):

     apertura 1,0 m²  ×  sprawność dzienna 0,49  ÷  0,1163 kWh/K  =  4,21 °C/(kWh/m²)

Czyli 4,2 NIE jest liczbą znikąd — odpowiada 10 rurom i 100 litrom.
Dlatego model dzienny zostaje, tylko przestaje być mieszany z sumą bufora.


────────────────────────────────────────────────────────────────────────────
 3. CO SIĘ ZMIENIA
────────────────────────────────────────────────────────────────────────────

a) KARTA „DZIŚ" — zmierzony przyrost z dzisiejszych próbek
     jeśli są próbki z dziś:  pokazuje ich sumę
     jeśli nie ma:            pokazuje model

b) KAFELKI W „BILANSIE" — wszystkie dotyczą DZIŚ
     ⚡ ZYSK DZIŚ        (było: ZYSK BRUTTO SŁOŃCA — suma narastająca)
     🌡️ ΔT PANEL − ZEWN.  (bez zmian)
     🚰 POBORY DZIŚ      (było: POBORY WODY CWU — narastające)
     🔮 PROGNOZA DZIŚ    (ten sam wzór co karta DZIŚ)

c) PROGNOZA — JEDEN WZÓR
     źródło: prognoza shortwave_radiation, a gdy jej brak — zachmurzenie
     4,2 °C/(kWh/m²), sufit 45 °C na dobę

d) KARTY 7 DNI — pik „do N W/m²" z prawdziwej prognozy GHI
     Wcześniej liczył z zachmurzenia. Teraz bierze max(shortwave_radiation).

e) ARKUSZ CZUJNIKI — rozdzielone „dziś" i „narastająco"
     Zysk dziś                +X.X °C
     Zysk narastająco         +X.X °C z N próbek
     ΔT panel − zewnątrz      +X.X °C
     Pobory wody dziś         N wykrytych · −X.X °C
     Pobory wody narastająco  N wykrytych · −X.X °C
     Prognoza zysku · dziś    +X.X °C

NIE ZMIENIONO: przelicznik 100 l, brak kWh, tryb serwa, ΔT, eksport CSV.


────────────────────────────────────────────────────────────────────────────
 4. ZNAJDŹ PROJEKT
────────────────────────────────────────────────────────────────────────────

$kand = Get-ChildItem "$env:USERPROFILE\Desktop" -Recurse -Filter "build.gradle.kts" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\app\\build\.gradle\.kts$' -and $_.FullName -notmatch '\\_backup' }

$src = $kand | Where-Object {
         (Select-String -Path $_.FullName -Pattern 'versionCode\s*=\s*3032' -Quiet) -and
         (Test-Path (Join-Path (Split-Path (Split-Path $_.FullName -Parent) -Parent) "app\src\main\java\com\sterownikco\pro\core\ServoPositionPreview.kt"))
       } | ForEach-Object { Split-Path (Split-Path $_.FullName -Parent) -Parent } | Select-Object -First 1
"Projekt: $src"


────────────────────────────────────────────────────────────────────────────
 5. KOPIA + WGRANIE
────────────────────────────────────────────────────────────────────────────

$bak = "$src\_backup-0.31.27-" + (Get-Date -Format "yyyyMMdd-HHmmss")
New-Item -ItemType Directory -Path $bak -Force | Out-Null

$pliki = @(
  "app\build.gradle.kts",
  "app\src\main\java\com\sterownikco\pro\core\SolarAnalytics.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Weather.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\sheets\Sensors.kt"
)
foreach ($p in $pliki) {
  Copy-Item -LiteralPath (Join-Path $src $p) -Destination (Join-Path $bak ($p -replace '\\','_'))
}
"Kopia: " + (Get-ChildItem $bak).Count + " plikow"

$zip = Get-ChildItem "$env:USERPROFILE\Downloads" -Filter "SterownikCO-Android-v0.31.27-DZIS*.zip" |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $src -Force

Select-String -Path "$src\app\build.gradle.kts" -Pattern 'versionCode|versionName'

Oczekiwane:  Kopia: 4 plikow / versionCode = 3033 / versionName = "v0.31.27"


────────────────────────────────────────────────────────────────────────────
 6. BUILD
────────────────────────────────────────────────────────────────────────────

Set-Location $src
.\gradlew.bat :app:assembleDebug --console=plain

APK: app\build\outputs\apk\debug\app-debug.apk


────────────────────────────────────────────────────────────────────────────
 7. CZEGO NIE SPRAWDZONO
────────────────────────────────────────────────────────────────────────────

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 3 plików Kotlina (0/0/0), importy SolarAnalytics + roundToInt
w Weather.kt obecne, grep „forecastKwh" = 0 (usunięte bez śladu).

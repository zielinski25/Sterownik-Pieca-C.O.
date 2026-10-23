════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.27  →  v0.31.28
 Panel solarny po ludzku: bez sum narastających, pomiar ≠ szacunek
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.27. Nadpisuje 3 pliki. Wersja: 3034 / "v0.31.28".

Zgłoszenie: „Narastającą jest zbędna, resztę zrób tak aby była łatwa do
zrozumienia dla nowej osoby".


────────────────────────────────────────────────────────────────────────────
 1. CO ZNIKA
────────────────────────────────────────────────────────────────────────────

„Zysk narastająco +93,1 °C z 206 próbek" — USUNIĘTE.
„Pobory wody narastająco" — USUNIĘTE.

Dlaczego: algorytm liczył KAŻDY wzrost jako zysk, a każdy spadek pomijał.
Drobne wahania czujnika o 0,3 °C w tę i z powrotem sumowały się jak realne
grzanie. 206 próbek dało 93,1 °C, choć woda była na 33 °C. Ta liczba nie
miała odpowiednika w rzeczywistości, więc nie ma jej na ekranie.


────────────────────────────────────────────────────────────────────────────
 2. BILANS SOLARNY — TAK WYGLĄDA TERAZ
────────────────────────────────────────────────────────────────────────────

     Panel teraz / powietrze           33.2 °C / 18.0 °C
     Różnica panel − powietrze                  +15.2 °C
     Zysk dziś · zmierzone                       +0.5 °C
     Prognoza na dziś · szacunek                 +1.5 °C
     Pobrania ciepłej wody                          brak

     Zysk = o ile °C ogrzała się woda w zbiorniku 100 l. „Zmierzone" to
     odczyt czujnika w zbiorniku, „szacunek" to wyliczenie z prognozy
     słońca — nie pomiar.

Zmiany nazw, żeby nie trzeba było znać skrótów:

     było                                  jest
     Panel teraz / zewnątrz           →    Panel teraz / powietrze
     ΔT panel − zewnątrz              →    Różnica panel − powietrze
     Zysk dziś                        →    Zysk dziś · zmierzone
     Prognoza zysku · dziś            →    Prognoza na dziś · szacunek
     Pobory wody dziś                 →    Pobrania ciepłej wody


────────────────────────────────────────────────────────────────────────────
 3. KARTY 7 DNI — WSZYSTKIE TO PROGNOZA
────────────────────────────────────────────────────────────────────────────

Wcześniej karta „DZIŚ" pokazywała pomiar, a pozostałe prognozę — w jednej
siatce pod tytułem „PROGNOZA POTENCJAŁU". Teraz wszystkie siedem kart to
prognoza, łącznie z DZIŚ. Pomiar dzisiejszy jest osobno, w „Zysku dziś".

Pod nagłówkiem doszedł podpis:

     📅 PROGNOZA POTENCJAŁU SOLARNEGO NA 7 DNI
     o ile °C może ogrzać się woda w zbiorniku 100 l · szacunek, nie pomiar


────────────────────────────────────────────────────────────────────────────
 4. KAFELKI NA ZAKŁADCE POGODA
────────────────────────────────────────────────────────────────────────────

     ⚡ ZYSK DZIŚ    🌡️ PANEL − POWIETRZE    🚰 POBRANIA DZIŚ    🔮 PROGNOZA DZIŚ

     było: ΔT PANEL - ZEWN.  →  jest: PANEL − POWIETRZE
     było: POBORY DZIŚ       →  jest: POBRANIA DZIŚ
     wartość: „2 razy" zamiast „2 poborów", „—" gdy brak


────────────────────────────────────────────────────────────────────────────
 5. ZNAJDŹ PROJEKT
────────────────────────────────────────────────────────────────────────────

$kand = Get-ChildItem "$env:USERPROFILE\Desktop" -Recurse -Filter "build.gradle.kts" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\app\\build\.gradle\.kts$' -and $_.FullName -notmatch '\\_backup' }

$src = $kand | Where-Object {
         (Select-String -Path $_.FullName -Pattern 'versionCode\s*=\s*3033' -Quiet) -and
         (Test-Path (Join-Path (Split-Path (Split-Path $_.FullName -Parent) -Parent) "app\src\main\java\com\sterownikco\pro\core\ServoPositionPreview.kt"))
       } | ForEach-Object { Split-Path (Split-Path $_.FullName -Parent) -Parent } | Select-Object -First 1
"Projekt: $src"


────────────────────────────────────────────────────────────────────────────
 6. KOPIA + WGRANIE
────────────────────────────────────────────────────────────────────────────

$bak = "$src\_backup-0.31.28-" + (Get-Date -Format "yyyyMMdd-HHmmss")
New-Item -ItemType Directory -Path $bak -Force | Out-Null
$pliki = @(
  "app\build.gradle.kts",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Weather.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\sheets\Sensors.kt"
)
foreach ($p in $pliki) { Copy-Item -LiteralPath (Join-Path $src $p) -Destination (Join-Path $bak ($p -replace '\\','_')) }
"Kopia: " + (Get-ChildItem $bak).Count + " plikow"

$zip = Get-ChildItem "$env:USERPROFILE\Downloads" -Filter "SterownikCO-Android-v0.31.28-CZYTELNE*.zip" |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $src -Force
Select-String -Path "$src\app\build.gradle.kts" -Pattern 'versionCode|versionName'

Oczekiwane:  Kopia: 3 plikow / versionCode = 3034 / versionName = "v0.31.28"


────────────────────────────────────────────────────────────────────────────
 7. BUILD
────────────────────────────────────────────────────────────────────────────

Set-Location $src
.\gradlew.bat :app:assembleDebug --console=plain

APK: app\build\outputs\apk\debug\app-debug.apk


────────────────────────────────────────────────────────────────────────────
 8. CZEGO NIE SPRAWDZONO
────────────────────────────────────────────────────────────────────────────

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 3 plików Kotlina (0/0/0), usunięty nieużywany parametr `m`
z SolarDayCard (sygnatura + 2 wywołania), diff drzewa = 3 pliki.

════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.25  →  v0.31.26
 WARIANT B: 200 kg → 100 l  oraz  koniec „~X kWh" na kafelkach
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.25. Nadpisuje 4 pliki. Wersja: 3032 / "v0.31.26".

Zgłoszenie użytkownika: „JAKIE 200 KG WODY".
Ustalenie z ARCHIWUM_SOLARNE_STAN.md §3: zbiornik ma 100 l, nie 200.
Decyzja: wariant B.


────────────────────────────────────────────────────────────────────────────
 1. CO SIĘ ZMIENIA
────────────────────────────────────────────────────────────────────────────

a) PRZELICZNIK: 200 kg → 100 l
     core/SolarAnalytics.kt, funkcja estKwh()
     było:  accumulatedGrossGain * 200 * 4.186 / 3600   → 0,2326 kWh/K
     jest:  accumulatedGrossGain * 100 * 4.186 / 3600   → 0,1163 kWh/K
     Wynik był zawyżony dokładnie dwa razy.

b) KONIEC „~X kWh" NA KAFELKACH — zostają same °C

   Zakładka POGODA → karta „BILANS & ZYSK KOLEKTORA SŁONECZNEGO":
     ⚡ ZYSK BRUTTO SŁOŃCA     było: +12.4°C / ~2.9 kWh   jest: +12.4°C
     🔮 PROGNOZA ZYSKU         było: +8.1°C / ~1.9 kWh    jest: +8.1°C
     karty 7 dni               było: +8.1°C / ~1.9 kWh    jest: +8.1°C

   Więcej → Czujniki → „BILANS SOLARNY":
     Zysk brutto słońca        było: +12.4°C · ~2.9 kWh   jest: +12.4°C
     Prognoza zysku            było: +8.1°C · ~1.9 kWh    jest: +8.1°C

   NIE ZMIENIONO: ΔT, pokrycie, pobory wody, suma GHI, eksport CSV.


────────────────────────────────────────────────────────────────────────────
 2. DLACZEGO USUWAMY kWh
────────────────────────────────────────────────────────────────────────────

ARCHIWUM_SOLARNE_STAN.md §10 i §13:

  · „ΔT × 0,1163 kWh/K" to równoważnik rzędu wielkości, nie zmierzony uzysk
    (termosyfon, brak przepływomierza),
  · GHI w rekordzie to PROGNOZA, nie pomiar — mianownik jest niepewny,
  · czujnik w połowie wysokości nie widzi stratyfikacji, więc energia jest
    raczej zawyżona,
  · apertura (~1,4 m²) i sprawność nigdy nie były zmierzone.

Trzy niepewności naraz → liczba kWh wyglądała na pomiar, którym nie była.
Stopnie Celsjusza są policzalne z czujnika. kWh nie były.


────────────────────────────────────────────────────────────────────────────
 3. ZNAJDŹ PROJEKT
────────────────────────────────────────────────────────────────────────────

$kand = Get-ChildItem "$env:USERPROFILE\Desktop" -Recurse -Filter "build.gradle.kts" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\app\\build\.gradle\.kts$' -and $_.FullName -notmatch '\\_backup' }

$src = $kand | Where-Object {
         (Select-String -Path $_.FullName -Pattern 'versionCode\s*=\s*3031' -Quiet) -and
         (Test-Path (Join-Path (Split-Path (Split-Path $_.FullName -Parent) -Parent) "app\src\main\java\com\sterownikco\pro\core\ServoPositionPreview.kt"))
       } | ForEach-Object { Split-Path (Split-Path $_.FullName -Parent) -Parent } | Select-Object -First 1
"Projekt: $src"


────────────────────────────────────────────────────────────────────────────
 4. KOPIA + WGRANIE
────────────────────────────────────────────────────────────────────────────

$bak = "$src\_backup-0.31.26-" + (Get-Date -Format "yyyyMMdd-HHmmss")
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

$zip = Get-ChildItem "$env:USERPROFILE\Downloads" -Filter "SterownikCO-Android-v0.31.26-KWH*.zip" |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $src -Force

Select-String -Path "$src\app\build.gradle.kts" -Pattern 'versionCode|versionName'

Oczekiwane:  Kopia: 4 plikow / versionCode = 3032 / versionName = "v0.31.26"


────────────────────────────────────────────────────────────────────────────
 5. BUILD
────────────────────────────────────────────────────────────────────────────

Set-Location $src
.\gradlew.bat :app:assembleDebug --console=plain

APK: app\build\outputs\apk\debug\app-debug.apk


────────────────────────────────────────────────────────────────────────────
 6. CZEGO NIE SPRAWDZONO
────────────────────────────────────────────────────────────────────────────

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 3 plików Kotlina (0/0/0), diff drzewa = dokładnie 4 pliki,
grep „kWh" w UI = 0 trafień (zostały tylko komentarze).

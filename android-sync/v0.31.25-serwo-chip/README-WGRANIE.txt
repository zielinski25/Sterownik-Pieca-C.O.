════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.24  →  v0.31.25
 KAFELEK SERWO: tryb jako pigułka obok tytułu, pod spodem dwie pozycje
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.24. Nadpisuje 3 pliki. Wersja: 3031 / "v0.31.25".


────────────────────────────────────────────────────────────────────────────
 1. JAK TERAZ WYGLĄDA
────────────────────────────────────────────────────────────────────────────

        ┌──────────────────────────┐
        │ SERWO   AUTO          ◉  │   ← AUTO w pigułce, innym stylem
        │ klapa 100%               │   ← duża wartość, 11 sp
        │ syberek 100%             │   ← podpis, 7,5 sp
        └──────────────────────────┘

    · tryb (AUTO / RĘCZNY) NIE jest już linią tekstu — to mała kolorowa
      pigułka obok tytułu, w stylu plakietki, nie etykiety,
    · pod spodem zostają tylko dwie pozycje, każda z własnym procentem,
    · kafelek dalej ma 3 linie i 64 dp — tyle samo, co POMPA i MIESZADŁO.

Kolory pigułki:
    AUTO     → cyjanowa (normalna praca)
    RĘCZNY   → żółta

Tryb BEZPIECZNY: pigułka znika (słowo ma 10 znaków i nie mieści się obok
tytułu), a zamiast wartości pojawia się:

        SERWO                    ◉
        BEZPIECZNY
        tryb bezpieczeństwa

(ramka czerwona — tak jak dotąd)


────────────────────────────────────────────────────────────────────────────
 2. ZNAJDŹ PROJEKT
────────────────────────────────────────────────────────────────────────────

$kand = Get-ChildItem "$env:USERPROFILE\Desktop" -Recurse -Filter "build.gradle.kts" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\app\\build\.gradle\.kts$' -and $_.FullName -notmatch '\\_backup' }

$src = $kand | Where-Object {
         (Select-String -Path $_.FullName -Pattern 'versionCode\s*=\s*3030' -Quiet) -and
         (Test-Path (Join-Path (Split-Path (Split-Path $_.FullName -Parent) -Parent) "app\src\main\java\com\sterownikco\pro\core\ServoPositionPreview.kt"))
       } | ForEach-Object { Split-Path (Split-Path $_.FullName -Parent) -Parent } | Select-Object -First 1

"Projekt: $src"


────────────────────────────────────────────────────────────────────────────
 3. KOPIA + WGRANIE
────────────────────────────────────────────────────────────────────────────

$bak = "$src\_backup-0.31.25-" + (Get-Date -Format "yyyyMMdd-HHmmss")
New-Item -ItemType Directory -Path $bak -Force | Out-Null

$pliki = @(
  "app\build.gradle.kts",
  "app\src\main\java\com\sterownikco\pro\ui\components\Chrome.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Dashboard.kt"
)
foreach ($p in $pliki) {
  Copy-Item -LiteralPath (Join-Path $src $p) -Destination (Join-Path $bak ($p -replace '\\','_'))
}
"Kopia: " + (Get-ChildItem $bak).Count + " plikow"

$zip = Get-ChildItem "$env:USERPROFILE\Downloads" -Filter "SterownikCO-Android-v0.31.25-CHIP*.zip" |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $src -Force

Select-String -Path "$src\app\build.gradle.kts" -Pattern 'versionCode|versionName'

Oczekiwane:  Kopia: 3 plikow / versionCode = 3031 / versionName = "v0.31.25"


────────────────────────────────────────────────────────────────────────────
 4. BUILD
────────────────────────────────────────────────────────────────────────────

Set-Location $src
.\gradlew.bat :app:assembleDebug --console=plain

APK: app\build\outputs\apk\debug\app-debug.apk

Copy-Item -LiteralPath "app\build\outputs\apk\debug\app-debug.apk" `
          -Destination "$env:USERPROFILE\Downloads\SterownikCO-v0.31.25-debug.apk" -Force


────────────────────────────────────────────────────────────────────────────
 5. CZEGO NIE SPRAWDZONO
────────────────────────────────────────────────────────────────────────────

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów obu plików Kotlina (0), diff drzewa = dokładnie 3 pliki, szerokości:
„SERWO" + pigułka „RĘCZNY" + ikona ≈ 71 dp przy ~87 dp dostępnych.

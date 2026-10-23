════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.23  →  v0.31.24
 KAFELEK SERWO: inna stylizacja — trzy linie jak u sąsiadów
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.23. Nadpisuje 4 pliki. Wersja: 3030 / "v0.31.24".


────────────────────────────────────────────────────────────────────────────
 1. CO I DLACZEGO
────────────────────────────────────────────────────────────────────────────

W v0.31.23 kafelek SERWO miał CZTERY linie, a POMPA i MIESZADŁO mają TRZY.
Na zrzucie widać, że SERWO jest przez to gęstsze i nierówne — tekst sięga
góry kafelka, nie ma oddechu.

Było (4 linie):
        SERWO
        klapa 100%
        syberek 100%
        tryb AUTO

Jest (3 linie — dokładnie tyle, ile u sąsiadów):
        SERWO
        klapa 100%
        syberek 100% · AUTO

    · wartość (11 sp)  — pozycja KLAPY w procentach
    · podpis (7,5 sp)  — pozycja SYBERKA + tryb na końcu
    · kafelek wraca do wysokości 64 dp (był podniesiony do 70 dp tylko po to,
      żeby zmieścić czwartą linię — teraz niepotrzebnie)

Przy trybie RĘCZNY:   "syberek 100% · RĘCZNY"
Przy BEZPIECZNY:      wartość "BEZPIECZNY", podpis "tryb bezpieczeństwa"
                      (w trybie awaryjnym pozycje i tak są wymuszone)
Gdy aplikacja czeka:  wartość "CEL 100%", podpis "klapa 45° · czekam"

DODATKOWO — kolor ramki:
    Było: tryb AUTO dawał zieloną ramkę i zieloną wartość, więc SERWO
    odcinało się od POMPY i MIESZADŁA, które przy normalnym stanie są
    neutralne. Teraz:
        AUTO        → neutralnie (jak sąsiedzi)
        RĘCZNY      → żółto
        BEZPIECZNY  → czerwono
        brak danych → żółto
    Kolor niesie teraz informację „coś odbiega", a nie „wszystko gra".


────────────────────────────────────────────────────────────────────────────
 2. ZNAJDŹ PROJEKT
────────────────────────────────────────────────────────────────────────────

Skrypt szuka folderu, który ma JEDNOCZEŚNIE: versionCode 3029 i plik
ServoPositionPreview.kt. To odsiewa starą, niekompletną kopię.

$kand = Get-ChildItem "$env:USERPROFILE\Desktop" -Recurse -Filter "build.gradle.kts" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\app\\build\.gradle\.kts$' -and $_.FullName -notmatch '\\_backup' }

$src = $kand | Where-Object {
         (Select-String -Path $_.FullName -Pattern 'versionCode\s*=\s*3029' -Quiet) -and
         (Test-Path (Join-Path (Split-Path (Split-Path $_.FullName -Parent) -Parent) "app\src\main\java\com\sterownikco\pro\core\ServoPositionPreview.kt"))
       } | ForEach-Object { Split-Path (Split-Path $_.FullName -Parent) -Parent } | Select-Object -First 1

"Projekt: $src"

Ma wyjść ścieżka do folderu, w którym budowałeś v0.31.23.


────────────────────────────────────────────────────────────────────────────
 3. KOPIA + WGRANIE
────────────────────────────────────────────────────────────────────────────

$bak = "$src\_backup-0.31.24-" + (Get-Date -Format "yyyyMMdd-HHmmss")
New-Item -ItemType Directory -Path $bak -Force | Out-Null

$pliki = @(
  "app\build.gradle.kts",
  "app\src\main\java\com\sterownikco\pro\ui\components\Chrome.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Dashboard.kt",
  "app\src\main\java\com\sterownikco\pro\ui\theme\Theme.kt"
)
foreach ($p in $pliki) {
  Copy-Item -LiteralPath (Join-Path $src $p) -Destination (Join-Path $bak ($p -replace '\\','_'))
}
"Kopia: " + (Get-ChildItem $bak).Count + " plikow"

$zip = Get-ChildItem "$env:USERPROFILE\Downloads" -Filter "SterownikCO-Android-v0.31.24-KAFELEK*.zip" |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
Expand-Archive -LiteralPath $zip.FullName -DestinationPath $src -Force

Select-String -Path "$src\app\build.gradle.kts" -Pattern 'versionCode|versionName'
Select-String -Path "$src\app\src\main\java\com\sterownikco\pro\ui\screens\Dashboard.kt" -Pattern 'tryb bezpieczeństwa'

Oczekiwane:
    Kopia: 4 plikow
    versionCode = 3030
    versionName = "v0.31.24"
    ...tryb bezpieczeństwa...


────────────────────────────────────────────────────────────────────────────
 4. BUILD
────────────────────────────────────────────────────────────────────────────

Set-Location $src
.\gradlew.bat :app:assembleDebug --console=plain

APK: app\build\outputs\apk\debug\app-debug.apk

Copy-Item -LiteralPath "app\build\outputs\apk\debug\app-debug.apk" `
          -Destination "$env:USERPROFILE\Downloads\SterownikCO-v0.31.24-debug.apk" -Force


────────────────────────────────────────────────────────────────────────────
 5. PO CZYM POZNASZ
────────────────────────────────────────────────────────────────────────────

 1. Ustawienia → APLIKACJA WEB PRO → v0.31.24-debug
 2. Pulpit → rząd trzech kafelków: POMPA, SERWO, MIESZADŁO mają teraz
    DOKŁADNIE tyle samo linii i tę samą wysokość.
 3. SERWO czyta się:
        SERWO
        klapa 100%
        syberek 100% · AUTO
    przy neutralnej ramce (bez zieleni, gdy tryb jest AUTO).


────────────────────────────────────────────────────────────────────────────
 6. CZEGO NIE SPRAWDZONO
────────────────────────────────────────────────────────────────────────────

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 3 plików Kotlina (0), diff drzewa = dokładnie 4 pliki, najdłuższy
podpis „syberek 100% · RĘCZNY" = 20 znaków ≈ 78 dp przy ~87 dp dostępnych.

════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.22  →  v0.31.23
 POPRAWKI: kafelek SERWO · kwadracik bez 8-sekundowej zwłoki
════════════════════════════════════════════════════════════════════════════

Ta paczka to NAKŁADKA na projekt, z którego zbudowałeś v0.31.22
(folder C:\Users\Rafcio\Desktop\SterownikCO-0.31.22).
Nadpisuje 7 plików. Nie dokłada żadnych nowych.

Wersja po zbudowaniu: versionCode 3029, versionName "v0.31.23".


────────────────────────────────────────────────────────────────────────────
 1. CO SIĘ ZMIENIŁO I DLACZEGO
────────────────────────────────────────────────────────────────────────────

[1] KAFELEK SERWO — było: „AUTO (100%)"
    Problem: jedna liczba przy trybie sugerowała, że serwo ma jedną pozycję.
    A są DWA niezależne serwa, a tryb to trzecia, osobna informacja.

    Jest:
        SERWO
        klapa 100%          ← duża wartość, pozycja klapy
        syberek 10%         ← pozycja syberka
        tryb AUTO           ← AUTO / RĘCZNY / BEZPIECZNY

    Kąt z centrali (klapa 0..180°, syberek 0..90°) jest przeliczany na procent
    — tylko procent jest porównywalny między dwoma różnymi serwami.
    Gdy aplikacja czeka na potwierdzenie pozycji, kafelek pokazuje:
        CEL 100% / klapa 45° · czekam / czekam na status

[2] KWADRACIK „BUFOR OFFLINE" — reaguje natychmiast
    Było: klik → toast „Potwierdzone przez sterownik (ACK)" → a kwadracik
    pusty przez kilka sekund, bo czytał tylko status, który leci co 8 s.
    Wyglądało jak zepsute; zaznaczał się dopiero po wyjściu i powrocie.

    Jest: klik → kwadracik zaznacza się OD RAZU, podpis „Wysyłanie… czekam
    na potwierdzenie centrali". Gdy przyjdzie świeży status zgodny z kliknięciem
    — podpis wraca do „Włączony · ustawienie zapisane w centrali".
    Jeśli w 12 s nie przyjdzie potwierdzenie — kwadracik wraca do stanu
    z centrali (czyli uczciwie pokazuje, że się nie udało).

[3] TO SAMO W DWÓCH INNYCH MIEJSCACH
    Ten sam błąd miał:
      · Kontrola → „Włączony antystop"
      · Mieszadło → „Automatyka włączona"
    Oba korzystają teraz z tego samego mechanizmu.

    NIE dotyczyło i nie zmienia: wykresów, kafelków Dashboardu, Telegrama —
    te zapisują się lokalnie i zawsze reagowały natychmiast.

[4] Wysokość kafelków szybkich: 64 → 70 dp
    Kafelek SERWO ma teraz czwartą linię tekstu. Przy 64 dp ostatnia linia
    była 2 dp od przycięcia. 70 dp daje zapas. Dotyczy trzech kafelków
    w rzędzie (POMPA / SERWO / MIESZADŁO) — wszystkie rosną tak samo.


────────────────────────────────────────────────────────────────────────────
 2. KOPIA ZAPASOWA (PowerShell)
────────────────────────────────────────────────────────────────────────────

$src = "C:\Users\Rafcio\Desktop\SterownikCO-0.31.22"
$d   = Get-Date -Format "yyyyMMdd-HHmmss"
$bak = "$src\_backup-0.31.23-$d"

New-Item -ItemType Directory -Path $bak -Force | Out-Null

$pliki = @(
  "app\build.gradle.kts",
  "app\src\main\java\com\sterownikco\pro\ui\components\Chrome.kt",
  "app\src\main\java\com\sterownikco\pro\ui\components\Kit.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Dashboard.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\sheets\Control.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\sheets\Offline.kt",
  "app\src\main\java\com\sterownikco\pro\ui\theme\Theme.kt"
)
foreach ($p in $pliki) {
  $cel = Join-Path $bak ($p -replace '\\', '_')
  Copy-Item -LiteralPath (Join-Path $src $p) -Destination $cel
}
"Kopia zapasowa: $bak"
(Get-ChildItem $bak).Count

Ma wyjść 7.


────────────────────────────────────────────────────────────────────────────
 3. WGRANIE PLIKÓW
────────────────────────────────────────────────────────────────────────────

$src = "C:\Users\Rafcio\Desktop\SterownikCO-0.31.22"
$zip = "$env:USERPROFILE\Downloads\SterownikCO-Android-v0.31.23-POPRAWKI.zip"

Expand-Archive -LiteralPath $zip -DestinationPath $src -Force

Select-String -Path "$src\app\build.gradle.kts" -Pattern 'versionCode|versionName'
Select-String -Path "$src\app\src\main\java\com\sterownikco\pro\ui\components\Kit.kt" -Pattern 'fun CheckRowEcho'

Oczekiwane:
    versionCode = 3029
    versionName = "v0.31.23"
    fun CheckRowEcho(


────────────────────────────────────────────────────────────────────────────
 4. BUDOWANIE
────────────────────────────────────────────────────────────────────────────

Set-Location "$env:USERPROFILE\Desktop\SterownikCO-0.31.22"
.\gradlew.bat :app:assembleDebug --console=plain

Koniec: BUILD SUCCESSFUL
APK: app\build\outputs\apk\debug\app-debug.apk

Kopia z czytelną nazwą:

Copy-Item -LiteralPath "app\build\outputs\apk\debug\app-debug.apk" `
          -Destination "$env:USERPROFILE\Downloads\SterownikCO-v0.31.23-debug.apk" -Force


────────────────────────────────────────────────────────────────────────────
 5. PO CZYM POZNASZ, ŻE DZIAŁA
────────────────────────────────────────────────────────────────────────────

 1. Ustawienia → APLIKACJA WEB PRO → ma być v0.31.23-debug.

 2. Pulpit → kafelek SERWO:
        SERWO
        klapa 100%
        syberek 10%
        tryb AUTO
    Nic nie jest ucięte wielokropkiem.

 3. Ustawienia → Bufor offline → klik w kwadracik:
    kwadracik zaznacza się NATYCHMIAST, podpis „Wysyłanie…", po 1–2 s
    „Włączony · ustawienie zapisane w centrali".

 4. Kontrola → Antystop oraz Mieszadło → Automatyka: tak samo, od razu.

 5. Jeśli wyłączysz internet w telefonie i klikniesz — po 12 s kwadracik
    wróci do poprzedniego stanu. To jest poprawne zachowanie, nie błąd.


────────────────────────────────────────────────────────────────────────────
 6. ROLLBACK
────────────────────────────────────────────────────────────────────────────

Skopiuj 7 plików z folderu _backup-0.31.23-... z powrotem na ich miejsca
(nazwy w backupie mają podkreślniki zamiast ukośników — rozpoznasz po nazwie)
i zbuduj ponownie: .\gradlew.bat :app:assembleDebug


────────────────────────────────────────────────────────────────────────────
 7. CZEGO NIE SPRAWDZONO
────────────────────────────────────────────────────────────────────────────

W środowisku, w którym powstała ta paczka, nie ma JDK, Kotlinc ani Gradle —
więc nie ma dowodu „kompiluje się". Sprawdzono:
  · bilans nawiasów każdego zmienionego pliku (0 różnicy),
  · każdą wstawkę asercją na dokładnie 1 wystąpienie kotwicy,
  · że zmienione są DOKŁADNIE 7 plików (diff całego drzewa),
  · szerokości tekstów: „klapa 100%" 10 znaków przy 11 sp ≈ 61 dp,
    „syberek 10%" i „tryb BEZPIECZNY" przy 7,5 sp ≈ 47 i 59 dp
    — wszystkie poniżej ~87 dp dostępnych w kafelku.

Jeśli build padnie — nie wgrywaj, odeślij linie zaczynające się od "e:".

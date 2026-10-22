════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.19  →  v0.31.20
 HISTORIA TELEMETRII: cache dzienny, mniej transferu, czytelne błędy
════════════════════════════════════════════════════════════════════════════

Baza: SterownikCO-Android-v0.31.18-build-20261016-094945 + nakładka v3.50.0
      (czyli dokładnie drzewo, z którego powstał Twój APK v0.31.19)

W paczce są CAŁE pliki, nie łatki. Rozpakowujesz do folderu `android`
i nadpisujesz 5 plików + dochodzą 2 nowe.


────────────────────────────────────────────────────────────────────────────
 1. CO SIĘ ZMIENIŁO I DLACZEGO
────────────────────────────────────────────────────────────────────────────

Objaw, który to naprawia:
  · wejście na Wykresy po 30 dniach = ~8 MB i 150 zapytań do Firebase,
  · każde wyjście z tła (sms, przełączenie aplikacji) = ponowne pobranie
    całego zakresu, nawet gdy stoisz na Pulpicie,
  · trzy szybkie zmiany zakresu = trzy równoległe pobrania, żadne nie
    anulowane,
  · jeden dzień z błędem = cicha dziura w wykresie bez informacji,
  · jeden przypadkowy HTTP 400 wyłączał stronicowanie do restartu aplikacji.

Co zostało zrobione (7 punktów):

  [F2] Historia pobierana TYLKO na zakładce Wykresy.
       Wcześniej `onResume()` wołał `loadRange()` bezwarunkowo — powrót
       z tła na Pulpicie też ściągał całe archiwum.

  [F3] Poprzednie pobranie jest anulowane (`historyJob?.cancel()`).
       Do tego 60-sekundowa blokada: powrót na Wykresy w ciągu minuty
       nie generuje żadnego zapytania. Przycisk odświeżania i zmiana
       zakresu mają `force = true`, więc działają natychmiast.

  [F1] Cache dzienny w pamięci aplikacji:
       `filesDir/telemetry/v1/RRRR-MM-DD.json` — jeden plik na dobę UTC,
       dokładnie w podziale shardów Firebase.
       · dzień zamknięty → czytany z pliku, ZERO ruchu sieciowego,
       · dzień bieżący → dociągany tylko od ostatniego zapisanego rekordu,
       · dzień bez pliku → pobierany w całości (nie wycinek!), żeby
         wejście na 24 h nie zatruło późniejszego widoku 30 dni.

  [F4] Błąd jednego dnia nie jest już cichy. Status pokazuje
       „brak 2 dni" / „dzień bez odświeżenia", a dane z pamięci zostają
       na wykresie. Chwilowy brak sieci nie czyści wykresu.

  [F5] Flaga trybu awaryjnego (`historyIndexFallback`) wygasa po 10 min
       i zapala się wyłącznie przy prawdziwym braku indeksu `ts`.
       Wcześniej substring "400" łapał każdy HTTP 400.

  [F9] Status mówi wprost, skąd wzięły się dane, np.:
       „1440 próbek · 1 dzień pobrany · 7 dni z pamięci
        · odcinki symulowane kreskowane"

  [+]  Wersja podbita do v0.31.20 (versionCode 3026), żebyś na telefonie
       widział, że naprawdę masz nowy APK.

Czego ta paczka NIE zmienia:
  · nie rusza Firebase, firmware'u ESP32 ani `/solar_daily`,
  · nie zmienia wyglądu wykresów ani sposobu liczenia serii,
  · nie kasuje żadnych danych na telefonie (cache to nowy katalog).

UWAGA: instaluj APK NA POPRZEDNI (ten sam pakiet `com.sterownikco.pro.debug`).
Wtedy dane aplikacji zostają i cache zaczyna się od zera sam z siebie.
Odinstalowanie aplikacji skasowałoby też ustawienia i sesję Firebase.


────────────────────────────────────────────────────────────────────────────
 2. ZANIM WGRASZ — kopia zapasowa (PowerShell)
────────────────────────────────────────────────────────────────────────────

Wklej całość do PowerShell:

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

$src = "C:\Users\Rafcio\Desktop\SterownikCO-Android-v0.31.18-build-20261016-094945\android"
$d   = Get-Date -Format "yyyyMMdd-HHmmss"
$bak = "C:\Users\Rafcio\Desktop\SterownikCO-Android-v0.31.18-build-20261016-094945\_backup-0.31.19-$d"

New-Item -ItemType Directory -Path $bak -Force | Out-Null

$pliki = @(
  "app\build.gradle.kts",
  "app\src\main\java\com\sterownikco\pro\core\AppModel.kt",
  "app\src\main\java\com\sterownikco\pro\core\RtdbClient.kt",
  "app\src\main\java\com\sterownikco\pro\core\Telemetry.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Charts.kt"
)
foreach ($p in $pliki) {
  $cel = Join-Path $bak ($p -replace '\\', '_')
  Copy-Item -LiteralPath (Join-Path $src $p) -Destination $cel
}

"Kopia zapasowa: $bak"
Get-ChildItem $bak | Select-Object Name, Length

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

Powinieneś zobaczyć 5 plików. Jeśli tak — możesz iść dalej.


────────────────────────────────────────────────────────────────────────────
 3. WGRANIE PLIKÓW
────────────────────────────────────────────────────────────────────────────

Ściągnij paczkę ZIP na Pulpit, potem w PowerShell:

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

$src  = "C:\Users\Rafcio\Desktop\SterownikCO-Android-v0.31.18-build-20261016-094945\android"
$zip  = "$env:USERPROFILE\Desktop\SterownikCO-Android-v0.31.20-HISTORIA-CACHE.zip"

Expand-Archive -LiteralPath $zip -DestinationPath $src -Force

# kontrola: musi być v0.31.20 i musi istnieć nowy plik
Select-String -Path (Join-Path $src "app\build.gradle.kts") -Pattern 'versionCode|versionName'
Test-Path (Join-Path $src "app\src\main\java\com\sterownikco\pro\core\TelemetryCache.kt")

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

Oczekiwany wynik:
  versionCode = 3026
  versionName = "v0.31.20"
  True

Jeśli `True` nie wyszło — ZIP rozpakował się w złe miejsce. Napisz, poprawimy.


────────────────────────────────────────────────────────────────────────────
 4. BUDOWANIE APK
────────────────────────────────────────────────────────────────────────────

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

cd "C:\Users\Rafcio\Desktop\SterownikCO-Android-v0.31.18-build-20261016-094945\android"
.\gradlew.bat :app:assembleDebug --console=plain

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

APK: app\build\outputs\apk\debug\app-debug.apk

Wymagania: JDK 17–21 (Gradle 8.9 nie wstaje na JDK 25), Android SDK 35.

TESTOWANIE (opcjonalne, ale polecam — to mój test cache):

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

.\gradlew.bat :app:testDebugUnitTest --tests "com.sterownikco.pro.core.TelemetryCacheTest" --console=plain

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

Raport: app\build\reports\tests\testDebugUnitTest\index.html
Testów jest 8: klucz doby UTC, zapis/odczyt z zachowaniem przerw w kanałach,
uszkodzony plik, zła wersja cache, prune 45 dni, delete/clear, pusty zapis.

Jeśli testy padną — APK i tak się zbuduje, ale napisz mi to. Testów nie
uruchamiaj „na wiarę”, wolę wiedzieć.


────────────────────────────────────────────────────────────────────────────
 5. PO CZYM POZNASZ, ŻE DZIAŁA
────────────────────────────────────────────────────────────────────────────

 1. Ustawienia → „APLIKACJA WEB PRO" → ma być v0.31.20-debug
    (build debug zawsze dopisuje „-debug").
    Jeśli dalej v0.31.19-debug — na telefonie jest stary APK.

 2. Pierwsze wejście na Wykresy po instalacji:
    pasek statusu pokaże „… dni pobranych” — to normalne, pamięć jest pusta.

 3. Wyjdź na Pulpit i wróć na Wykresy:
    ma być „… dni z pamięci”, BEZ pobierania. Wcześniej leciało całe 8 MB.

 4. Wyjdź z aplikacji do innej apki, wróć na Wykresy w ciągu minuty:
    brak jakiegokolwiek nowego ruchu.

 5. Przełącz 24 H → 30 DNI:
    dojdą brakujące dni, reszta z pamięci. Drugie przełączenie — już nic.

 6. Włącz tryb samolotowy, wejdź na Wykresy:
    wykres zostaje, status mówi „dzień bez odświeżenia” albo „brak N dni”.

 7. Sprawdź zajętość: Ustawienia → Aplikacje → SterownikCO → Pamięć.
    30 dni mieści się w ~4,5 MB.


────────────────────────────────────────────────────────────────────────────
 6. JAK WRÓCIĆ DO 0.31.19
────────────────────────────────────────────────────────────────────────────

Skopiuj 5 plików z folderu `_backup-0.31.19-<data>` z powrotem na swoje
miejsca (podkreślniki w nazwie to oryginalne ścieżki) i zbuduj ponownie:

  app_build.gradle.kts                     → app\build.gradle.kts
  app_src_main_java_com_sterownikco_pro_core_AppModel.kt    → core\AppModel.kt
  app_src_main_java_com_sterownikco_pro_core_RtdbClient.kt  → core\RtdbClient.kt
  app_src_main_java_com_sterownikco_pro_core_Telemetry.kt   → core\Telemetry.kt
  app_src_main_java_com_sterownikco_pro_ui_screens_Charts.kt → ui\screens\Charts.kt

Plik `TelemetryCache.kt` możesz zostawić — bez wpisów w AppModel.kt jest
nieużywany i nic nie robi.


────────────────────────────────────────────────────────────────────────────
 7. ZAWARTOŚĆ PACZKI
────────────────────────────────────────────────────────────────────────────

  README-WGRANIE.txt      ten plik
  ZMIANY.diff             pełny diff (do wglądu, nie jest potrzebny do wgrania)

  app/build.gradle.kts                                            (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/AppModel.kt           (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/RtdbClient.kt         (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/Telemetry.kt          (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/TelemetryCache.kt     (NOWY)
  app/src/main/java/com/sterownikco/pro/ui/screens/Charts.kt       (zmieniony)
  app/src/test/java/com/sterownikco/pro/core/TelemetryCacheTest.kt (NOWY)

════════════════════════════════════════════════════════════════════════════

════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.19  →  v0.31.22
 HISTORIA: cache dzienny · TERMINAL: auto-stop · SERWO: procenty
 NOWE: przełącznik „Bufor offline"
════════════════════════════════════════════════════════════════════════════

Baza: SterownikCO-Android-v0.31.18-build-20261016-094945 + nakładka v3.50.0
      (czyli dokładnie drzewo, z którego powstał Twój APK v0.31.19)

W paczce są CAŁE pliki, nie łatki. Rozpakowujesz do folderu `android`
i nadpisujesz 10 plików + dochodzą 3 nowe.

WAŻNE: „Bufor offline" wymaga **firmware v3.54.0** w centrali. Sama aplikacja
nie wystarczy — stara centrala nie zna tej komendy i nie wysyła stanu.
Kolejność: najpierw firmware, potem APK (albo odwrotnie — aplikacja pokaże
wtedy „Ta wersja firmware nie zgłasza tego ustawienia", ale nic się nie zepsuje).


────────────────────────────────────────────────────────────────────────────
 1. CO SIĘ ZMIENIŁO I DLACZEGO
────────────────────────────────────────────────────────────────────────────

Objaw, który naprawia historia:
  · wejście na Wykresy po 30 dniach = ~8 MB i 150 zapytań do Firebase,
  · każde wyjście z tła (sms, przełączenie aplikacji) = ponowne pobranie
    całego zakresu, nawet gdy stoisz na Pulpicie,
  · trzy szybkie zmiany zakresu = trzy równoległe pobrania, żadne nie anulowane,
  · jeden dzień z błędem = cicha dziura w wykresie bez informacji,
  · jeden przypadkowy HTTP 400 wyłączał stronicowanie do restartu aplikacji.

Co zostało zrobione (10 punktów):

  [F2] Historia pobierana TYLKO na zakładce Wykresy.

  [F3] Poprzednie pobranie jest anulowane (`historyJob?.cancel()`), plus
       60-sekundowa blokada. Przycisk odświeżania i zmiana zakresu mają
       `force = true`, więc działają natychmiast.

  [F1] Cache dzienny w pamięci aplikacji:
       `filesDir/telemetry/v1/RRRR-MM-DD.json` — jeden plik na dobę UTC.
       · dzień zamknięty → czytany z pliku, ZERO ruchu sieciowego,
       · dzień bieżący → dociągany tylko od ostatniego zapisanego rekordu,
       · dzień bez pliku → pobierany w całości (nie wycinek!), żeby wejście
         na 24 h nie zatruło późniejszego widoku 30 dni.

  [F4] Błąd jednego dnia nie jest już cichy. Status pokazuje „brak 2 dni" /
       „dzień bez odświeżenia", a dane z pamięci zostają na wykresie.

  [F5] Flaga trybu awaryjnego (`historyIndexFallback`) wygasa po 10 min
       i zapala się wyłącznie przy prawdziwym braku indeksu `ts`.

  [F9] Status mówi wprost, skąd wzięły się dane, np.:
       „1440 próbek · 1 dzień pobrany · 7 dni z pamięci".

  [T]  Terminal DLOG przestaje pytać Firebase po 5 minutach bez dotknięcia
       ekranu. Wcześniej otwarty i zapomniany terminal wysyłał zapytanie
       co 5 s BEZ KOŃCA — 720 zapytań na godzinę. Dotknięcie (albo samo
       przewinięcie) wznawia odbiór. Gdy odbiór jest wstrzymany, pasek
       statusu mówi to wprost.

  [P]  Kafelek SERWO na Pulpicie pokazuje pozycje w PROCENTACH, nie stopniach.
       Było: „klapa 180° · syberek 90°" → jest: „klapa 100% · syberek 100%".
       Duża liczba obok („AUTO (100%)") już wcześniej była w procentach.
       Przelicznik: klapa 180° = 100%, syberek 90° = 100%.
       Arkusz „Serwo" bez zmian (tam procent jest duży, kąt mały).

  [B]  NOWE: Ustawienia → „Bufor offline". Przełącznik + liczniki:
       ile stron zapisano na flash centrali, ile próbek zapisano i ile wróciło
       do Firebase. Stan czytany wyłącznie ze statusu centrali (`spool_rt`),
       a nie z tego, co kliknąłeś — po kliknięciu widać ACK, po chwili stan.
       Wymaga firmware v3.54.0.

  [+]  Wersja podbita do v0.31.22 (versionCode 3028), żebyś na telefonie
       widział, że naprawdę masz nowy APK.

Czego ta paczka NIE zmienia:
  · nie rusza Firebase, reguł bazy ani `/solar_daily`,
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
  "app\src\main\java\com\sterownikco\pro\core\PiecState.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Charts.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Dashboard.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Lists.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\Overlays.kt",
  "app\src\main\java\com\sterownikco\pro\ui\screens\sheets\System.kt"
)
foreach ($p in $pliki) {
  $cel = Join-Path $bak ($p -replace '\\', '_')
  Copy-Item -LiteralPath (Join-Path $src $p) -Destination $cel
}

"Kopia zapasowa: $bak"
Get-ChildItem $bak | Select-Object Name, Length

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

Powinieneś zobaczyć 10 plików. Jeśli tak — możesz iść dalej.


────────────────────────────────────────────────────────────────────────────
 3. WGRANIE PLIKÓW
────────────────────────────────────────────────────────────────────────────

Ściągnij paczkę ZIP na Pulpit, potem w PowerShell:

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

$src  = "C:\Users\Rafcio\Desktop\SterownikCO-Android-v0.31.18-build-20261016-094945\android"
$zip  = "$env:USERPROFILE\Desktop\SterownikCO-Android-v0.31.22-BUFOR-OFFLINE.zip"

Expand-Archive -LiteralPath $zip -DestinationPath $src -Force

# kontrola: musi być v0.31.22 i muszą istnieć pliki, których wcześniej nie było
Select-String -Path (Join-Path $src "app\build.gradle.kts") -Pattern 'versionCode|versionName'
Test-Path (Join-Path $src "app\src\main\java\com\sterownikco\pro\core\TelemetryCache.kt")
Test-Path (Join-Path $src "app\src\main\java\com\sterownikco\pro\ui\screens\sheets\Offline.kt")

--------8<--------8<--------8<--------8<--------8<--------8<--------8<--------

Oczekiwany wynik:
  versionCode = 3028
  versionName = "v0.31.22"
  True
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

Jeśli testy padną — APK i tak się zbuduje, ale napisz mi to.


────────────────────────────────────────────────────────────────────────────
 5. PO CZYM POZNASZ, ŻE DZIAŁA
────────────────────────────────────────────────────────────────────────────

 1. Ustawienia → „APLIKACJA WEB PRO" → ma być v0.31.22-debug
    (build debug zawsze dopisuje „-debug").
    Jeśli dalej 0.31.19-debug — na telefonie jest stary APK.

 2. Pierwsze wejście na Wykresy po instalacji:
    pasek statusu pokaże „… dni pobranych" — to normalne, pamięć jest pusta.

 3. Wyjdź na Pulpit i wróć na Wykresy:
    ma być „… dni z pamięci". Z sieci pójdzie co najwyżej ogon dzisiejszego
    dnia (jeden mały request), a nie całe 8 MB jak wcześniej.

 4. Wyjdź z aplikacji do innej apki, wróć na Wykresy w ciągu minuty:
    brak jakiegokolwiek nowego ruchu.

 5. Przełącz 24 H → 30 DNI: dojdą brakujące dni, reszta z pamięci.

 6. Włącz tryb samolotowy, wejdź na Wykresy:
    wykres zostaje, status mówi „dzień bez odświeżenia" albo „brak N dni".

 7. Sprawdź zajętość: Ustawienia → Aplikacje → SterownikCO → Pamięć.
    30 dni ≈ 5 MB, pełne 45 dni (limit prune) ≈ 7,5 MB.

 8. Więcej → Terminal diagnostyczny, zostaw otwarty BEZ dotykania przez 5 min.
    Pasek statusu: „Podgląd wstrzymany po 5 min bez dotykania ekranu".
    Dotknij — odbiór wraca.

 9. Pulpit → kafelek SERWO: podpis ma mieć procenty, np. „klapa 100% · syberek 100%".

10. Ustawienia → „Bufor offline":
    · na firmware v3.54.0 — przełącznik działa, pod nim liczniki,
    · na starszym firmware — napis „Ta wersja firmware nie zgłasza tego ustawienia",
      przełącznik nieaktywny. To nie błąd aplikacji.


────────────────────────────────────────────────────────────────────────────
 6. JAK WRÓCIĆ DO 0.31.19
────────────────────────────────────────────────────────────────────────────

Skopiuj 10 plików z folderu `_backup-0.31.19-<data>` z powrotem na swoje
miejsca (podkreślniki w nazwie to oryginalne ścieżki) i zbuduj ponownie:

  app_build.gradle.kts                     → app\build.gradle.kts
  app_src_main_java_com_sterownikco_pro_core_AppModel.kt    → core\AppModel.kt
  app_src_main_java_com_sterownikco_pro_core_RtdbClient.kt  → core\RtdbClient.kt
  app_src_main_java_com_sterownikco_pro_core_Telemetry.kt   → core\Telemetry.kt
  app_src_main_java_com_sterownikco_pro_core_PiecState.kt   → core\PiecState.kt
  app_src_main_java_com_sterownikco_pro_ui_screens_Charts.kt → ui\screens\Charts.kt
  app_src_main_java_com_sterownikco_pro_ui_screens_Dashboard.kt → ui\screens\Dashboard.kt
  app_src_main_java_com_sterownikco_pro_ui_screens_Lists.kt → ui\screens\Lists.kt
  app_src_main_java_com_sterownikco_pro_ui_screens_Overlays.kt → ui\screens\Overlays.kt
  app_src_main_java_com_sterownikco_pro_ui_screens_sheets_System.kt → ui\screens\sheets\System.kt

Pliki `TelemetryCache.kt`, `Offline.kt` i `TelemetryCacheTest.kt` możesz zostawić
— bez wpisów w pozostałych plikach są nieużywane i nic nie robią.


────────────────────────────────────────────────────────────────────────────
 7. ZAWARTOŚĆ PACZKI
────────────────────────────────────────────────────────────────────────────

  README-WGRANIE.txt      ten plik
  ZMIANY.diff             pełny diff od 0.31.19 (do wglądu, nie do wgrania)

  app/build.gradle.kts                                            (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/AppModel.kt           (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/PiecState.kt          (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/RtdbClient.kt         (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/Telemetry.kt          (zmieniony)
  app/src/main/java/com/sterownikco/pro/core/TelemetryCache.kt     (NOWY)
  app/src/main/java/com/sterownikco/pro/ui/screens/Charts.kt       (zmieniony)
  app/src/main/java/com/sterownikco/pro/ui/screens/Dashboard.kt    (zmieniony)
  app/src/main/java/com/sterownikco/pro/ui/screens/Lists.kt        (zmieniony)
  app/src/main/java/com/sterownikco/pro/ui/screens/Overlays.kt     (zmieniony)
  app/src/main/java/com/sterownikco/pro/ui/screens/sheets/Offline.kt (NOWY)
  app/src/main/java/com/sterownikco/pro/ui/screens/sheets/System.kt (zmieniony)
  app/src/test/java/com/sterownikco/pro/core/TelemetryCacheTest.kt (NOWY)

════════════════════════════════════════════════════════════════════════════

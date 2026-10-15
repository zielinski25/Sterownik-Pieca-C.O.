# Praca w sieci lokalnej centrali — także bez internetu (Android + panel WWW)

Telefony podłączone do tej samej sieci co centrala, np. sieci wewnętrznej budynku albo hotspotu centrali `PiecCO_AP`, łączą się z nią bezpośrednio. Status, polecenia, alarmy i wykresy działają wtedy także bez internetu i bez Firebase. Aplikacja pokazuje, że jest w sieci centrali: znacznik **● LAN** w górnym pasku i opis „sieć lokalna, bez Firebase” na pulpicie.

Pełne sterowanie lokalne wymaga **firmware v3.47.0** (`firmware-patch/v3.38.0-local-html`), które dodaje `/api/lan/status`, `/api/lan/cmd` i `/api/lan/ack`. Ze starszym firmware aplikacja w sieci lokalnej tylko odczytuje status, a polecenia wysyła przez Firebase.

## Jak to działa w aplikacji

- **Wykrywanie centrali.** Aplikacja sprawdza kolejno:
  1. adres wpisany ręcznie;
  2. adres zapamiętany z ostatniego statusu Firebase (pole `ip`);
  3. hotspot centrali `192.168.4.1`.

  Znaleziony adres sprawdza przy każdym odczycie, czyli co 8 s. Gdy centrali nie ma w sieci, sprawdza ją co najwyżej co 30 s, z krótkimi limitami, więc poza domem nie spowalnia odczytów z Firebase.
- **Status.** Gdy centrala jest w sieci, status idzie z `GET /api/lan/status`, czyli ten sam JSON co `/piec/status`. Firebase nie jest wtedy odpytywany, co jest szybsze i oszczędza transfer.
- **Polecenia.** W sieci centrali polecenie idzie przez `POST /api/lan/cmd` z potwierdzeniem `GET /api/lan/ack`.
  - W centrali wykonuje je ten sam wykonawca co polecenia z Firebase.
  - Chroni je ten sam token poleceń, przesyłany w nagłówku `X-Piec-Token`.
  - Toast pokazuje „Potwierdzone przez sterownik (LAN, … ms)”.
  - Gdy polecenie na pewno nie dotarło do centrali (zerwane połączenie, starszy firmware), aplikacja wysyła je przez Firebase.
  - Powtórzony identyfikator nie wykona polecenia drugi raz.
- **Logowanie.** W sieci centrali logowanie do Firebase nie jest potrzebne. Gdy automatyczne logowanie nie udaje się z braku internetu, okno logowania nie zasłania pulpitu; aplikacja ponowi logowanie co 2 min.
- **Alarmy w tle.** `AlarmMonitorService` przy działającym internecie czyta status z Firebase, tak jak dotąd. Bez internetu albo bez sesji czyta go z centrali w sieci lokalnej.
  - Czuwanie może działać także na telefonie zalogowanym tylko w sieci lokalnej.
  - Adres centrali jest dostępny także przed odblokowaniem telefonu po restarcie.
  - Wyciszenie alarmu trafia do centrali przez LAN.
- **Wykresy bez internetu.** Wykres łączy kopię dób z telefonu z lokalnym archiwum centrali: `/api/history`, a dla 7 i 30 dni także `/api/history/long`. Archiwum uzupełnia tylko czas po ostatniej próbce z kopii. Znacznik wykresu: **LAN · CENTRALA**.
- **Ustawienia.** Menu sesji (ikona ⋯ w górnym pasku) → sekcja **SIEĆ LOKALNA (LAN)**:
  - stan połączenia;
  - przełącznik „Połączenie lokalne”;
  - opcjonalny adres IP centrali;
  - przycisk „Sprawdź teraz”.

## Panel WWW (`Piec.html`, osadzony w firmware v3.47.0)

- **Strona z centrali (`http://<IP centrali>/`):**
  - czyta status i wysyła polecenia przez `/api/lan/*`, bez logowania i bez internetu;
  - pokazuje znacznik **● LAN · IP** i opis „sieć lokalna — bez Firebase”;
  - wykresy dalej idą z lokalnego archiwum centrali.
- **Panel otwarty spoza centrali** (plik albo strona http): gdy centrala jest w tej samej sieci, pokazuje baner „Sterownik jest w tej samej sieci” z przyciskiem **OTWÓRZ PANEL LOKALNY**. Na stronach https przeglądarka blokuje takie sprawdzenie.
- **Wykres z Firebase w panelu:**
  - też zapamiętuje pobrane doby, w IndexedDB przeglądarki;
  - dociąga z Firebase tylko nowe rekordy, a doby kompletne bierze z pamięci;
  - bez internetu pokazuje kopię z oznaczeniem **PAMIĘĆ PRZEGLĄDARKI**.
  - Przycisk „Wykresy: pobierz historię od nowa” w menu sesji kasuje tylko tę kopię.
  - Historia w Firebase nie jest zmieniana.
- **Naprawione zapytania historii:** dotąd token był doklejany drugim „?” do zapytań z `orderBy`, więc zapytanie po indeksie `ts` nigdy się nie udawało. Panel zawsze pobierał wtedy całe doby albo dostawał odmowę.

## Zastosowanie

Po pobraniu zmian uruchom z katalogu głównego repozytorium. Kolejność ma znaczenie, bo zmiana LAN wymaga zmiany pamięci dób:

```powershell
.\android-sync\Apply-Telemetry-Day-Cache.ps1
.\android-sync\Apply-LAN-Local-Mode.ps1
cd .\android
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug
& "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r ".\app\build\outputs\apk\debug\app-debug.apk"
```

Skrypty rozpoznają, czy zmiany już są w źródłach (np. po `git pull` tej gałęzi), i niczego nie nadpisują. Firmware v3.47.0 zbuduj i wgraj osobno, według `firmware-patch/v3.38.0-local-html/README.md`.

## Bezpieczeństwo

- **Adresy centrali:** tylko prywatne adresy IPv4. Ruch idzie bez proxy i bez przekierowań.
- **Weryfikacja odpowiedzi:** aplikacja akceptuje wyłącznie odpowiedź wyglądającą jak status centrali, nie dowolne urządzenie pod tym adresem.
- **Polecenia lokalne:** wymagają tokenu poleceń w nagłówku. Trasa poleceń nie ma CORS, więc obca strona w przeglądarce nie wyśle polecenia.
- **Odczyt statusu:** status centrali w sieci lokalnej jest dostępny bez tokenu, tak jak dotychczasowe `/api/status`.

## Weryfikacja (sandbox)

- **Kompilacja:** pakiet `core` (`AppModel`, `RtdbClient`, `Prefs`, nowe pliki) skompilowany w Kotlin 2.0 na atrapach API Androida. Wynik: 0 błędów, 0 ostrzeżeń w zmienionych plikach.
- **Testy jednostkowe:** 32 testy JVM przechodzą (`EspLocalHistoryTest`, `LanLinkTest`, `AlarmMonitorStartGateTest`, `LocalControllerAddressTest`, `TelemetryDayStoreTest`).
- **Test integracyjny prawdziwego `RtdbClient`** na emulowanym firmware v3.47.0 i starszym. Sprawdzono 15 przypadków, m.in.:
  - status;
  - wykrywanie;
  - polecenie z potwierdzeniem i odrzucenie;
  - zły token, zajętość;
  - brak sieci, starszy firmware;
  - brak podwójnego wysłania.
- **Brak pełnego builda:** pełnego builda Gradle nie dało się tu uruchomić (brak Android SDK). Uruchom `:app:testDebugUnitTest :app:assembleDebug` lokalnie.

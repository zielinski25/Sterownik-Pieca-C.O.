# Sieć lokalna bez internetu, wspólne rozpalanie, wykresy lokalne i Firebase — baza v3.38.0, źródło v3.47.1

Ta skumulowana łatka zachowuje lokalne archiwum wykresu ESP, używa timestampów RTDB dla wykresu Firebase i dodaje jedno wspólne sterowanie rozpalaniem serw oraz mieszadła. `Piec.html` jest osadzony w przygotowanym CPP. Głównego `Cenatrala Pieca.zip` w katalogu repozytorium nie zmieniamy.

## v3.47.1 — naprawa kompilacji i zapis HEALTH co 5 min

**v3.47.1:** `FIRMWARE_VERSION` przechodzi z **v3.47.0** na **v3.47.1** (PATCH).

1. **Naprawa kompilacji (błąd z v3.46.0).** Sześć miejsc używało niezadeklarowanej nazwy `rozpalanieTrybAktywne` zamiast zmiennej `rozpalanieTrybAktywny`. PlatformIO przerywał kompilację błędem `'rozpalanieTrybAktywne' was not declared in this scope`. To ta sama poprawka co commit `ce3e4c6` w gałęzi `arena/539ab1b6-sterownik-pieca-c-o`. Logika bez zmian.
2. **Zmiana z v3.45.1 (HEALTH co 5 min).** Wersja v3.45.1 powstała obok linii v3.46.0/v3.47.0, więc jej zmiany tu nie było. Heartbeat (`tag=HEALTH`) i snapshot pamięci (`tag=HEALTH-A`) zapisują się co 5 minut zamiast co 30 sekund (`HEALTH_LOG_INTERVAL_MS = 300000`, osobny znacznik `lastHealthLog`). Wcześniej te rekordy zajmowały ok. 91% logu na flashu. Checkpoint diagnostyczny pozostaje co 30 s, treść rekordów jest bez zmian, a pierwszy rekord po starcie pojawia się po 5 min. Zmianę odtworzono według opisu w nagłówku pliku v3.45.1, bo jego źródła nie ma w repozytorium.

Budowanie: w projekcie PlatformIO z `Cenatrala Pieca.zip` podmień `src/main_centrala.cpp` na plik z tego katalogu i uruchom `pio run -e esp32-s3-n16r8`. Projekt potrzebuje też lokalnej biblioteki DHT22 w podfolderze `lib/dhtESP32-rmt/` (github.com/junkfix/dhtESP32-rmt). Nie ma jej w ZIP-ie projektu.

### Windows: `Wgraj-Firmware.ps1`

```powershell
$fw = "$env:USERPROFILE\SterownikCO\firmware-patch\v3.38.0-local-html\Wgraj-Firmware.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -File $fw              # tylko kompilacja
powershell -NoProfile -ExecutionPolicy Bypass -File $fw -Wgraj OTA   # wgranie przez sieć lokalną
powershell -NoProfile -ExecutionPolicy Bypass -File $fw -Wgraj USB   # wgranie kablem
```

Co robi skrypt:

1. Sam znajduje projekt PlatformIO centrali, czyli `platformio.ini` z `[env:esp32-s3-n16r8]`. Szuka w Dokumentach, na pulpicie, w Pobranych, w OneDrive i w folderze użytkownika. Pomija stare kopie w `Pulpit\SterownikCO\archiwum`.
2. Znajduje `pio.exe`, także w `%USERPROFILE%\.platformio\penv` instalowanym przez VS Code.
3. Gdy brakuje `lib\dhtESP32-rmt`, pobiera tę bibliotekę.
4. Robi kopię dotychczasowego `src\main_centrala.cpp` w `kopie-firmware\`, wstawia plik z tego katalogu i kompiluje. Gdy kompilacja się nie uda, przywraca poprzedni plik.
5. Kopiuje `firmware.bin` do folderu `Pulpit\SterownikCO` jako `firmware_<wersja>.bin`, na przykład do GitHub Release i `/update`. Starsze `firmware*.bin` z tego folderu przenosi do `Pulpit\SterownikCO\archiwum`.
6. Wgrywa firmware na centralę tylko z `-Wgraj USB` albo `-Wgraj OTA`.

Pozostałe opcje: `-Projekt <katalog>`, `-Port COM5`, `-AdresOta 192.168.x.x`, `-BezPulpitu`.

Platforma pioarduino 55.03.38-1 wymaga pakietu `platformio` w wersji 6.1.19. Gdy w `penv` jest inna wersja (np. 6.2.0 po aktualizacji PlatformIO IDE), przy każdym `pio run` platforma próbuje przeinstalować rdzeń. Na Windowsie to się nie udaje, bo działający `pio.exe` blokuje swoje pliki, a w logu pojawia się `Failed to install Python dependencies into penv`. Skrypt wtedy instaluje `platformio==6.1.19`, gdy `pio` nie działa, i kompiluje jeszcze raz.

**Test skryptu (2026-10-15, GitHub Actions `windows-latest`):** środowisko jak u użytkownika, czyli projekt z `Cenatrala Pieca.zip` w `Dokumenty\PlatformIO\Projects\Cenatrala Pieca` (bez `lib\dhtESP32-rmt`) i świeże PlatformIO 6.2.0 z oficjalnego instalatora, poza PATH. W Windows PowerShell 5.1 skrypt:

- znalazł projekt i `pio.exe`;
- pobrał bibliotekę DHT;
- zrobił kopię `main_centrala_v3.36.2_….cpp`;
- naprawił wersję rdzenia (6.2.0 → 6.1.19);
- skompilował v3.47.1 (**SUCCESS**, `firmware.bin` 2 365 440 B) i położył `firmware_v3.47.1.bin` na pulpicie.

Drugie uruchomienie w PowerShell 7 rozpoznało podmieniony plik. Wgrywania (`-Wgraj`) nie testowano, bo w CI nie ma urządzenia.

**Kompilacja (2026-10-15):** workflow `.github/workflows/firmware-build.yml` (GitHub Actions) uruchamia `pio run -e esp32-s3-n16r8`. Środowisko: platforma pioarduino 55.03.38-1, `platformio.ini` z `Cenatrala Pieca.zip`, `lib/dhtESP32-rmt` 2.0.2. Wynik: **SUCCESS**. RAM 23,3% (76 328 B), Flash 75,2% (2 364 678 B z 3 145 728 B). Nie wgrywano firmware na urządzenie.

## v3.47.0 — praca w sieci lokalnej, także bez internetu (LAN API)

Telefon w tej samej sieci co centrala (sieć wewnętrzna budynku albo hotspot `PiecCO_AP`) steruje piecem bezpośrednio. Firebase nie jest do tego potrzebny, więc wszystko działa także wtedy, gdy padnie internet.

| Trasa | Odpowiednik w Firebase | Opis |
|---|---|---|
| `GET /api/lan/status` | `/piec/status` | Ten sam JSON co status w Firebase (wspólny builder `zbudujJsonStatusPelny()`), plus pola `lan`, `lan_api` i `internet`. Ma nagłówki CORS tylko do odczytu, żeby panel otwarty spoza centrali mógł ją wykryć. |
| `POST /api/lan/cmd` | `/piec/cmd` | Pola formularza `id` i `cmd`. Token jest w nagłówku `X-Piec-Token` (ten sam `CMD_TOKEN` co w Firebase). |
| `GET /api/lan/ack?id=…` | `/piec/ack` | Wynik polecenia: `done`, `ok`, `error`. |

- **Ten sam wykonawca poleceń:** wykonawca wydzielono 1:1 z `checkFirebaseCommands()` do `wykonajKomendePanelu()`. Zmieniły się wyłącznie nazwy zmiennych wyniku.
- **To samo zadanie:** polecenia z LAN wykonuje to samo zadanie `tgTaskFn` co polecenia z Firebase, więc obie drogi nigdy nie działają równolegle.
  - Handler HTTP tylko kopiuje polecenie do jednego slotu chronionego spinlockiem, bez alokacji pod blokadą.
  - Slot jest odbierany co 100 ms, także w przerwie pętli. Polecenie nie czeka na Firebase.
- **Powtórzony `cmdId`** nie wykonuje polecenia drugi raz, tylko zwraca jego wynik.
- **Zajęty slot:** inne polecenie w kolejce dostaje odpowiedź 409.
- **Ochrona przed obcymi stronami:** `/api/lan/cmd` celowo nie ma CORS. Własny nagłówek wymusza w przeglądarce preflight, więc obca strona nie wyśle polecenia (ochrona CSRF).
- **Log:** wartość `tg_config` jest w logu ukryta (`tg_config <ukryte>`).
- **Bufor PSRAM statusu Firebase:** 2048 → 3072 B. JSON ma ok. 1,9 KB; to zapas na nowe pola.
- **Starszy firmware:** na nieznane trasy odpowiada przekierowaniem na „/”. Aplikacja i panel traktują to jak brak `/api/lan/*`: odczyt z `/api/status`, polecenia tylko przez Firebase.
- **Panel z ESP (`Piec.html`):**
  - czyta status i wysyła polecenia przez `/api/lan/*`, bez logowania do Firebase;
  - pokazuje znacznik **● LAN · IP**.
- **Panel otwarty spoza centrali (plik lub http):** gdy wykryje centralę w tej samej sieci, pokazuje baner z linkiem do panelu lokalnego.

## Jedno wspólne sterowanie „Rozpalaniem”

- `rozpalanie_serwa` uruchamia jednocześnie sekwencję serw i czasowy override mieszadła; `rozpalanie_stop` wyłącza cały tryb. `ustaw rozpalanie 1/0` pozostaje aliasem start/stop.
- Start jest odrzucany przy nieświeżej/niewiarygodnej temperaturze, aktywnym alarmie bezpieczeństwa albo temperaturze pieca powyżej 30°C. Zimne okno `<25°C` udostępnia przycisk, ale samo nie uruchamia wymuszeń.
- Rozpalenie jest potwierdzone, gdy temperatura pieca przekroczy 30°C **lub** wzrośnie o co najmniej 5°C względem odczytu zapisanego przy starcie.
- Bez potwierdzenia tryb kończy się po 30 min; po potwierdzeniu nadal obowiązuje bezwzględny limit 60 min od startu. Tryb nie jest odtwarzany po restarcie.
- Alarmy bezpieczeństwa i dymu nadal zatrzymują wymuszenia. Po normalnym zakończeniu sterowanie wraca do zwykłej automatyki serw i mieszadła.
- Status publikuje `rozpalanie`, `rozpalanie_potwierdzone` i `rozpalanie_pozostalo_s`; interfejsy blokują start na starszym firmware bez pól potwierdzających obsługę wspólnego trybu.

## Zachowanie wykresu ESP (bez zmiany źródła lokalnego)

- **6 h / 24 h:** pobieranie `GET /api/history` (lokalny bufor minutowy zapisany w `/hist_short.csv`).
- **7 dni / 30 dni:** pobieranie `GET /api/history/long` oraz łączenie z ostatnimi 24 h z `/api/history`.
- Na stronie serwowanej przez ESP wykres, jego próbki, pompa i odświeżanie nadal pochodzą wyłącznie z lokalnych endpointów. Błąd lokalnego API nie uruchamia fallbacku Firebase.
- Istniejące lokalne archiwum nie zapisuje `t_trociny` ani `dym`; te opcjonalne serie pozostają bez lokalnej historii.
- Lokalna ścieżka i jej rozmieszczanie próbek pozostają wzorcem i nie korzystają z danych statusowych Firebase.

## Naprawiona ścieżka wykresu Firebase

- Wykres dodaje wyłącznie rekordy zapisane w archiwum RTDB, używając ich `ts`. Odpytanie `/piec/status` co 4 s aktualizuje bieżący panel, ale nie tworzy historycznego punktu wykresu.
- Oś X, ticki, powiększanie, przewijanie i crosshair po stronie Firebase odnoszą się do czasu próbek, a nie do ich indeksu w tablicy.
- Zakres `od–do` jest ustawiany na granicach minutowych. Wyświetlany opis pokazuje granice aktywnego okna.
- Co minutę strona doczytuje tylko końcówkę archiwum z krótkim nakładaniem, a rekordy łączy i deduplikuje po ID (z fallbackiem `ts + seq`). Nie pobiera ponownie całego zakresu 7/30 dni.
- Konwersja serw do procentów zachowuje wartości ułamkowe jak parser lokalnego CSV, a dolne granice `t_panel` i `t_trociny` odpowiadają zakresom wspólnego filtra wykresu.

Dla porównania 6 h / 24 h wybierz te same serie i ustaw ten sam zakres. Lokalne i Firebase-owe archiwa minutowe mogą różnić się dostępnością pojedynczych zapisów. Dla 7/30 dni dokładna zgodność rozdzielczości nie jest gwarantowana: lokalne długie archiwum jest zdarzeniowe, a RTDB zapisuje próbki minutowe. Porównanie wymaga też takich samych filtrów i ustawień osi.

## Wersja i bezpieczeństwo zmian

**v3.47.0:** `FIRMWARE_VERSION` przechodzi z **v3.46.0** na **v3.47.0**.
- Doszły trasy `/api/lan/*`, wspólny builder statusu i wspólny wykonawca poleceń.
- Logika poleceń, Firebase, telemetria, LCD i dotychczasowe trasy są bez zmian.
- Kontrola odwrotna potwierdza, że poza tymi wydzieleniami i dodatkami plik jest bajt w bajt taki sam jak v3.46.0.

Wcześniej: źródłowe `FIRMWARE_VERSION` przeszło z **v3.45.0** na **v3.46.0**. Zmieniono wyłącznie wspólną logikę rozpalania oraz jej lokalne/Firebase API i opisy; zabezpieczenia alarmowe pozostają aktywne. Nie zmieniano głównego `Cenatrala Pieca.zip`. Nie wykonano kompilacji firmware, flashowania ani poleceń do urządzenia.

## Zastosowanie

Łatka skumulowana jest względem bazowego `src/main_centrala.cpp` v3.38.0. W osobnej kopii tej bazy sprawdź i zastosuj:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Nie stosuj jej do zmodyfikowanego `Cenatrala Pieca.zip` w głównym checkoutcie. Budowanie, instalacja i ewentualne OTA są osobnymi działaniami.

## Weryfikacja

v3.47.0:

- **Kontrola odwrotna:** odwrócenie wydzieleń, zmiany nazw i dodatków (z oryginalnym `Piec.html`) daje plik bajt w bajt identyczny z v3.46.0.
- **Kompilacja:** nowy kod LAN skompilowano `g++ -std=gnu++17 -Wall -Wextra -Wshadow` na atrapach API zgodnych z ESPAsyncWebServer 3.11.2. Wynik: bez ostrzeżeń.
- **Test funkcjonalny slotu poleceń** sprawdza:
  - przyjęcie i ponowienie;
  - zajętość (409);
  - wykonanie;
  - brak podwójnego wykonania;
  - ucieczkę znaków w JSON;
  - ukrycie tokenu w logu.
- **Testy strony w Node/jsdom:**
  - strona z ESP startuje w trybie LAN bez internetu i bez logowania;
  - polecenie z UI idzie przez `/api/lan/cmd` z tokenem i potwierdzeniem;
  - zachowanie ze starszym firmware;
  - baner wykrycia centrali.
- **Bez kompilacji całego firmware:** brak platformy PlatformIO w środowisku. Przed wgraniem uruchom `pio run`.

Wcześniej (v3.46.0):

- JavaScript strony sprawdzony parserem Node.js.
- Testy regresyjne sprawdzają: brak tworzenia punktów z pollingu statusu, deduplikację archiwum, przyrostowe doczytywanie, pozycję X opartą o `ts` oraz niezależną ścieżkę lokalną.
- `WIFI_LOCAL_PANEL_HTML` jest porównywany bajt w bajt z aktualnym `Piec.html`.
- Skumulowaną łatkę sprawdzono przez dry-run i zastosowanie do śledzonej bazy v3.38.0; wynik odtwarza przygotowany CPP.
- Nie wykonano `pio run`, uploadu, flashowania ani komend sterujących piecem.

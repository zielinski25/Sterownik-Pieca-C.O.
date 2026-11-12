# Testy panelu LCD na PC

Ten katalog pozwala uruchomić **prawdziwy** `src/main.cpp` panelu (razem z
`dashboard_lvgl_poc.cpp` i `lvgl_port.cpp`) na zwykłym komputerze, bez płytki
ESP32. PlatformIO go nie widzi i nie kompiluje — nie wpływa na firmware.

```
./uruchom_testy.sh                 # wszystko: testy jednostkowe + symulator (kilka sekund; pierwszy raz dłużej)
./uruchom_testy.sh --szybko        # tylko testy jednostkowe (sam g++, 1 s)
./uruchom_testy.sh dwie_centrale   # jeden scenariusz, z pełnym logiem Seriala na ekranie
SANITIZER=1 ./uruchom_testy.sh     # to samo z AddressSanitizer + UBSan (wolniej)
./sprawdz_naglowki_esp32.sh        # składnia i typy prawdziwymi nagłówkami ESP32 (patrz niżej)
```

Wymaga: `g++` (C++17), `gcc`, `ar`, `git`, Linux albo WSL. Przy pierwszym
uruchomieniu skrypt pobiera z GitHuba LVGL 8.3.11 i ArduinoJson 7.2.1 do
katalogu podręcznego (`$LCD_TESTY_CACHE`, domyślnie `/tmp/lcd_testy_host`).
Logi Seriala i zrzuty ekranu (`.bmp`) z każdego scenariusza trafiają do
`<katalog podręczny>/wyniki/`.

`src/secrets.h` nie jest potrzebny — gdy go nie ma, testy dostają atrapę z
wymyślonymi hasłami (atrapa WiFi haseł nie sprawdza).

## Co jest w środku

| Plik | Co robi |
|---|---|
| `test_diagnostyka.cpp` | Testy jednostkowe `src/diagnostyka_panelu.h` (kod HTTP, dławik logu, strażnik świeżości, wybór najsilniejszego AP, znacznik centrali, próg baterii). |
| `mock/` | Atrapy nagłówków Arduino/ESP-IDF (`Arduino.h`, `WiFi.h`, `TFT_eSPI.h`, `Preferences.h`, `esp_*.h`…). Stan „płytki” opisuje `mock/atrapa.h`. |
| `symulator_panelu.cpp` | Uruchamia `setup()` i `loop()` firmware'u, podstawia mu „centrale” (lokalny serwer HTTP) i sprawdza log oraz to, co trafiło na ekran. Odpowiedź `/api/status` ma te same pola, kolejność i format liczb co `wifiZbudujJsonStatus()` w firmware centrali 3.58.3 (ok. 1,8 kB). |
| `uruchom_testy.sh` | Buduje i uruchamia całość. |
| `sprawdz_naglowki_esp32.sh` | Sprawdza składnię i typy źródeł panelu prawdziwymi nagłówkami arduino-esp32 / ESP-IDF / TFT_eSPI (bez atrap). |

LVGL i ArduinoJson są **prawdziwe** (te same wersje co w `platformio.ini`),
LVGL jest budowany z `include/lv_conf.h` tego projektu. Ekran to bufor
480×320, do którego LVGL rysuje tak jak na płytce.

## Scenariusze

| Nazwa | Sytuacja |
|---|---|
| `dwie_centrale` | Dwie centrale nadają tę samą sieć (prawdziwa daleko, testowa obok). Hipoteza nr 1 dla zgłoszenia „kafelki się nie aktualizują”. |
| `dwie_centrale_piec_blizej` | To samo, ale panel wisi przy piecu. |
| `zla_odpowiedz` | Centrala odpowiada, ale treści nie da się użyć: pole `nan`, HTTP 500, pusta odpowiedź, odpowiedź ucięta. |
| `brak_centrali_na_starcie` | Panel włączony bez centrali w zasięgu; centrala wstaje później. |
| `zanik_i_powrot` | Centrala znika w trakcie pracy, wraca, a potem wraca jako inna płytka (inny BSSID). |
| `nieudany_post` | Jedno nieudane polecenie z dotyku. |
| `bateria_i_log` | Minuta zwykłej pracy: objętość logu, wskaźnik baterii przy samym USB. |
| `skan_zawieszony` | Skan WiFi, który się nie kończy. |
| `ustawienia_milcza` | `/api/status` działa, `/api/ustawienia-get` nie odpowiada przez 3 minuty (sytuacja z logu panelu z 2026-11-06). |
| `gola_plytka` | Stanowisko z logów z 2026-11-11: centrala bez czujników (wszędzie −1) z symulacją jednego czujnika; zmiana i wyłączenie symulacji. |

## Czego to NIE sprawdza

To nie jest emulator ESP32. Wynik „zaliczone” znaczy: logika panelu zachowuje
się tak, jak opisują sprawdzenia, **przy założeniach z atrap**. Nie znaczy, że
firmware kompiluje się dla ESP32 ani że działa na płytce.

- **Kompilacja dla ESP32.** Atrapy mają te same sygnatury co arduino-esp32 3.x,
  ale to nie jest toolchain ESP32. Zgodność z prawdziwymi nagłówkami sprawdza
  osobny skrypt (następny rozdział); buildu PlatformIO nie zastępuje nic.
- **Radio.** Model WiFi (`mock/atrapa.cpp`) to trzy reguły:
  - `WiFi.begin()` z BSSID łączy się tylko z tym nadajnikiem;
  - bez BSSID, przy `WIFI_FAST_SCAN` (domyślne w arduino-esp32), wygrywa sieć
    na **najniższym kanale** — to *założenie* wynikające z opisu „scan will
    end after find SSID match AP” i kolejności kanałów, nie pomiar;
  - bez BSSID, przy `WIFI_ALL_CHANNEL_SCAN`, wygrywa najsilniejsza.

  Wynik „v1.1.0 łączy się z nie tą centralą” w scenariuszu `dwie_centrale`
  wynika wprost z drugiej reguły.
- **Czasy sieci.** Zegar płytki jest sztuczny (`delay()` przesuwa `millis()`),
  a HTTP idzie po lokalnym gnieździe. Opóźnienia lwIP, zakłócenia kanału AP
  i czasy z prawdziwego logu nie są odtwarzane.
- **Sprzęt.** TFT_eSPI, dotyk, brzęczyk, deep sleep, OTA, ekran Wykresy —
  atrapy przyjmują wywołania, ale ich nie modelują.
- **Watchdog.** Atrapa tylko odnotowuje, że przy takim przebiegu watchdog
  zadania (15 s) by zadziałał; nie restartuje „płytki”.

## Sprawdzenie prawdziwymi nagłówkami ESP32

`./sprawdz_naglowki_esp32.sh` kompiluje `main.cpp`, `dashboard_lvgl_poc.cpp`
i `lvgl_port.cpp` w trybie „tylko sprawdź” (`-fsyntax-only`) z prawdziwymi
nagłówkami: rdzeń i biblioteki arduino-esp32 (3.0.7; `RDZEN=3.3.8` dla
nowszego), ESP-IDF 5.1.4 z konfiguracją płytki `esp32`, newlib, TFT_eSPI
2.5.43, LVGL 8.3.11, ArduinoJson 7.4.3, flagi `-D` z `platformio.ini`.
Kompilatorem jest clang z pakietu Pythona `ziglang` (ma cel xtensa); wszystko
pobiera się z PyPI i GitHuba do katalogu podręcznego (ok. 250 MB).

Wykrywa to, czego atrapy nie widzą: literówkę w nazwie funkcji, złą liczbę
albo typ argumentów, brakujący `#include`. Nie jest to build PlatformIO:
inny kompilator (clang zamiast GCC) i biblioteka C++, bez linkowania i bez
pliku `.bin`; przy `RDZEN=3.3.8` nagłówki rdzenia 3.3.8 stoją na ESP-IDF 5.1,
więc dwa typy znane tylko nowszemu IDF są podstawione zastępczo (szczegóły
w nagłówku skryptu).

## Porównanie ze starszą wersją

Ten sam zestaw scenariuszy można puścić na innym katalogu źródeł w trybie
obserwacji (bez oceniania, każde sprawdzenie pokazuje `tak`/`NIE`):

```
mkdir -p /tmp/stary && unzip -q "../../LCD Centrali.zip" -d /tmp/stary     # v1.1.0
SRC=/tmp/stary/src OBSERWACJE=1 ./uruchom_testy.sh
```

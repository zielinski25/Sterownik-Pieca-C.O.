# Centrala testowa na ESP32-C3 (zastępnik centrali dla panelu LCD)

Firmware, który **udaje centralę pieca** na ESP32-C3: stawia to samo WiFi (AP
`PiecCO_AP`, brama `192.168.4.1`) i wystawia **ten sam kontrakt HTTP**, który
czyta panel LCD (`LCD Centrali.zip` → `src/main.cpp`). Pod spodem nie ma
przekaźników ani czujników — jest **symulacja** pieca, pompy, serwa i czujnika
dymu, plus osobny panel WWW do wymuszania awarii i ustawiania czasu.

Cel: **sprawdzić panel LCD „na biurku”, bez prawdziwej centrali i bez S3.**

---

## 1. Dlaczego nie da się tam wgrać prawdziwej centrali (i co zamiast tego)

Prawdziwa centrala (`Cenatrala Pieca.zip` → `src/main_centrala.cpp`) jest
zbudowana pod **ESP32-S3 N16R8** i tego nie da się „tylko przekompilować” na C3:

| | prawdziwa centrala (S3 N16R8) | ESP32-C3 |
|---|---|---|
| flash | 16 MB (`board_build.flash_size = 16MB`, partycje dual-OTA po 3 MB + coredump) | 4 MB |
| PSRAM | 8 MB OPI, **36 miejsc alokacji** (`ps_malloc`, `MALLOC_CAP_SPIRAM`) — m.in. 24 h historii i 7 dni historii długiej, bufor logów, bufory Telegram/Remote | **brak** |
| rdzenie | 2 × Xtensa LX7 (osobne zadania FreeRTOS: pompa, serwo, logi, Telegram, Firebase) | 1 × RISC-V |
| kod | 1,65 MB w jednym pliku, ~28 tras HTTP, 9 bibliotek (FirebaseClient, ESPAsyncWebServer+AsyncTCP, RTClib, BMP280, OneWire/DS18B20) | — |
| I/O | ~20 GPIO: 8× DS18B20, DHT22, BMP280, MQ-2 (ADC), RTC I2C, 2× serwo, przekaźniki, buzzer | 6 użytecznych GPIO |

Wniosek: na C3 brakuje **pamięci**, nie flag kompilacji. Sensownym ruchem jest
**centrala-zastępnik**: ta sama „rozmowa” po HTTP, dane z symulacji.

---

## 2. Co dokładnie udaje (kontrakt 1:1 z `main_centrala.cpp`)

| Endpoint | Wywołuje panel | Odpowiedź |
|---|---|---|
| `GET /api/status?panel_fw=..&reset_powod=..` | co 2 s | JSON ~1,4 KB, 68 kluczy (wszystkie, które czyta panel + pola diagnostyczne) |
| `GET /api/ustawienia-get` | co 5 s | JSON z 25 polami (`POLA[]` panelu + `wybor`/`trybSerwa`/`antystopWlaczony`) |
| `POST /api/ustawienia-set` | dotknięcie +/- | `{"ok":true,"wartosc":..}` albo `{"ok":false,"error":".."}` — **te same komunikaty i zakresy** co w centrali |
| `POST /api/servo-manual` | suwak klapa/syberka | odmowa, gdy `trybSerwa != 2` („tryb serwa nie jest reczny”), jak w produkcie |
| `POST /api/pompa-manual` | przycisk pompy | `auto` / `wl` / `wyl`, override 60 min + `pompa_override_min` w statusie |
| `GET /api/history` | ekran Wykresy | CSV **14 kolumn**, czas w kolumnie 13 |
| `GET /api/history/long` | Wykresy (Dzień/Tydzień) | CSV **35 kolumn**, czas w kolumnie 34 |
| `GET /panel-ota.bin` | tylko gdy `panel_ota_dostepna` | 404 (symulator nie robi OTA) |
| **`GET /api/rtc-sync`** | — (to robi WWW) | ustawienie czasu, ten sam format co centrala |
| **`GET /api/diag`, `GET /api/test`** | — | diagnostyka i wstrzykiwanie awarii (tylko symulator) |

Szczegóły zgodności (nazwy kluczy, kolejność kolumn CSV, zakresy walidacji i
treści komunikatów błędów) są odwzorowane z `main_centrala.cpp` i **pilnowane
testem** — patrz sekcja 8.

### Symulacja — co się dzieje „w piecu”

* piec C.O. grzeje się od ciągu: `klapa` (0–180°) + `syberka` (0–90°), τ ≈ 150 s
  (realny piec: dziesiątki minut — dynamika jest **~5× szybsza**, żeby test nie
  wymagał czekania godziny),
* pompa: strategia 1 = cykl `czasOn`/`czasOff` (z blokadą przy `t < 55 °C` i
  histerezą 5 °C), 2 = histereza `tempOn`/`tempOff`, 3 = „automatyczna”
  (stabilizacja powrotu), plus **minimalny czas stanu 60 s** (anty-short-cycling),
* serwo: AUTO z histerezą wokół `tempZadServo` i krokami `skokKlapy`/
  `skokSyberka` (jeden krok na 30 s, bo w centrali `servo()` też jest wołane
  rzadko — przy zmianach fazy pompy), RECZNY z suwaka, BEZPIECZNY (klapa 0,
  syberka 90°),
* dym: MQ-2 ~240 ADC w spoczynku; alarm zapala się po `dymCzasStabilizacji`
  sekundach nad `progAlarmDym`, gaśnie po `dymCzasOff` sekundach pod progiem
  (`progAlarmDym - 30`) — dokładnie jak `pozar()` w centrali,
* alarmy CO / panelu: progi `progAlarmTemp` / 95 °C z histerezą + **pozycja
  bezpieczna serw przy każdym alarmie**,
* słońce, temperatura zewnętrzna, wilgotność i ciśnienie zmieniają się z godziną,
* historia: próbka co 60 s (6 h) + agregat godzinny sr/min/max (5 dni).

---

## 3. Sprzęt i wgranie

Potrzebujesz **tylko**: ESP32-C3 (DevKitM-1, SuperMini, WROOM-02 — dowolny z
4 MB flash) i kabel USB. **Nic nie trzeba lutować** — firmware nie używa GPIO.

```bash
cd centrala-testowa-c3
pio run -t upload          # wgrywa po USB
pio device monitor         # konsola diagnostyczna, 115200
```

Bez PlatformIO (samo esptool):

```bash
pio run                    # zbuduje .pio/build/esp32-c3/firmware.bin
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 firmware.bin
```

Po starcie w konsoli zobaczysz m.in.:

```
[AP] wstalo  SSID="PiecCO_AP"  IP=192.168.4.1  kanal=6
[HTTP] nasluchuje na 192.168.4.1:80
[CZAS] brak zapisanego czasu — panel pokaze "--:--".
```

---

## 3a. Upload: „This chip is ESP32 not ESP32-C3. Wrong --chip argument?”

Objaw (Windows, PlatformIO):

```
Looking for upload port...
Auto-detected: COM5
Serial port COM5
Connecting.....
A fatal error occurred: This chip is ESP32 not ESP32-C3. Wrong --chip argument?
```

Co to znaczy: **build jest dobry** (RAM ~20%, flash ~60%), ale esptool połączył się
z **niewłaściwą płytką**. `Auto-detected` bierze pierwszy wolny port COM, a na
biurku zwykle wisi też **panel LCD — i on jest ESP32** (projekt panelu ma env
`esp32dev`), nie C3. Dobrze, że esptool odmówił: inaczej nadpisałby firmware panelu.

Kolejność działań:

1. **Lista portów:**
   ```powershell
   pio device list
   ```

2. **Który port to C3?** (skrypt sprawdza układ na każdym porcie esptool-em
   z PlatformIO — nic nie instaluje):
   ```powershell
   powershell -ExecutionPolicy Bypass -File narzedzia\znajdz_port.ps1 -ChipId
   ```
   Wypisze np. `COM3  CH340 ... ESP32 (klasyczny — to NIE jest C3; pewnie panel LCD)`
   i `COM7  USB JTAG/serial ... ESP32-C3  <-- TO TA PŁYTKA`.

3. **Wpisz port na sztywno** w `platformio.ini` (są tam gotowe, zakomentowane linie):
   ```ini
   upload_port = COM7
   monitor_port = COM7
   ```
   albo jednorazowo: `pio run -t upload --upload-port COM7`.

   Typowy przypadek: `pio device list` pokazuje **tylko jeden** port, np.
   `COM5 ... USB-SERIAL CH340 (VID:PID=1A86:7523)`, a `chip_id` mówi
   `Chip is ESP32...` — czyli pod USB wisi **panel (ESP32)**, a C3 nie jest
   podłączona wcale. Sam CH340 o niczym nie przesądza (C3 SuperMini też go ma),
   rozstrzyga dopiero `chip_id`.

4. **Jeśli C3 nie ma na liście portów:**
   * kabel USB „tylko do ładowania” (bez linii danych) — najczęstsza przyczyna,
   * brak sterownika: **CH340/CH343** (C3 SuperMini) lub **CP210x** (DevKitM),
   * C3 z natywnym USB (złącze USB, nie UART): przytrzymaj **BOOT**, naciśnij i
     puść **RESET**, puść **BOOT** — wejdzie w tryb downloadu i pokaże się port,
   * port zajęty przez `pio device monitor` → zamknij monitor (Ctrl+C).

5. **Ręczne sprawdzenie jednego portu** (bez skryptu). Najodporniejsza forma —
   PlatformIO samo znajduje swoje narzędzia (nie trzeba znać ścieżek):
   ```powershell
   pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --port COM5 chip_id
   ```
   Właściwy wynik zaczyna się od `Chip is ESP32-C3 (QFN32)`.
   Klasyczny ESP32 (np. panel LCD) odpowie `Chip is ESP32-D0WD...`.

   > Jeśli wklejasz komendę z czatu/edytora, **uważaj na „ozdabianie” tekstu** —
   > raz zdarzyło się, że `esptool.py` wkleiło się jako link `[esptool.py](http://esptool.py)`
   > i Python szukał pliku o nazwie z nawiasami (`can't open file ... [esptool.py]`).
   > Wtedy po prostu wpisz komendę ręcznie albo użyj `pio pkg exec` powyżej.

> Uwaga: **nie** dodawaj `-DCORE_DEBUG_LEVEL` / `board = esp32dev` do tego env,
> żeby „jakoś się wgrało” — to zmieni chip docelowy i firmware nie ruszy na C3.

---

## 4. Pierwsze uruchomienie (2 minuty)

1. **Ustaw czas.** C3 nie ma RTC ani internetu (działa jako AP), więc po
   włączeniu zasilania zegar jest nieznany → panel pokaże `--:--`.
   Wejdź telefonem/PC w WiFi `PiecCO_AP` (hasło `KociolCO2026`), otwórz
   `http://192.168.4.1` — strona **sama ustawia czas z przeglądarki** przy
   wejściu (jest też przycisk). Alternatywy:
   `python3 narzedzia/sprawdz_api.py --czas`, albo w konsoli Serial:
   `czas 2026-11-05 14:30:00`.
2. **Włącz panel LCD.** On łączy się z `PiecCO_AP` sam (ma to zapisane
   w `WIFI_SSID`/`WIFI_PASS`) i po chwili zacznie odpytywać `/api/status`.
   *Uwaga:* panel trzyma w pamięci RTC ostatni BSSID/kanał centrali — po
   podmianie urządzenia może potrzebować jednego–dwóch cykli wybudzenia, żeby
   połączyć się „na świeżo”.
3. **Sprawdź, że dane lecą.** W konsoli C3 pojawi się
   `[PANEL] zgloszona wersja: v1.1.0` i `[PANEL] powod restartu: deepsleep`,
   a na stronie WWW zniknie „lacze…” i pojawią się temperatury.

---

## 5. Checklista testu panelu LCD

| Co kliknąć na panelu | Czego oczekiwać |
|---|---|
| dashboard | temperatury zmieniają się co 2 s; PIEC CO rośnie, gdy klapa się otwiera |
| kafelek POMPA → AUTO/WŁ/WYŁ | stan zmienia się od razu, pod spodem licznik override (60 min) |
| SERWO → tryb AUTO | klapa/syberka ruszają się co ~30 s w stronę 60 °C zadanego |
| SERWO → RĘCZNY + suwak | serwo jedzie do zadanej pozycji; po powrocie do AUTO wraca automatyka |
| SERWO → BEZPIECZNA | klapa 0°, syberka 90° |
| USTAWIENIA → +/- | wartość wraca z centrali (nie „kłamie”), po restarcie C3 zostaje (NVS) |
| USTAWIENIA → wartości graniczne | te same komunikaty odmowy co na produkcie |
| Wykresy → Godziny / Dzień | wykres rysuje się z CSV (po ~10 min jest już kilka punktów) |
| ALARM DYMU (strona WWW) | po 20 s (`dymCzasStabilizacji`) panel wchodzi w alarm dymu; po wyłączeniu — gaśnie po 120 s |
| ALARM CO / PANELU | ekran alarmowy + serwo w pozycji bezpiecznej |
| wyłączenie C3 | panel pokazuje brak połączenia / zamrożone dane, nie zawiesza się |

Wszystkie „awarie” wstrzykujesz z `http://192.168.4.1` (sekcja *Wymus awarie*)
albo z Seriala (`dym 1`, `co 1`, `panel 1`, `deszcz 1`).

---

## 6. Znalezione przy okazji (do decyzji — nie zmieniałem produktu)

1. **Niezgodność jednostek klapa/syberka między centralą a panelem.**
   `/api/status` w centrali podaje **stopnie** (`pozycjaKlapa` 0–180,
   `pozycjaSyberek` 0–90 — patrz komentarz `[v3.31.24]` w `main_centrala.cpp`),
   a panel wyświetla je z dopiskiem `%` (`snprintf(bufor, ..., "%d %%", klapa)`
   w `dashboard_lvgl_poc.cpp`) i wysyła z suwaka **procenty**, które centrala
   przelicza na stopnie. Efekt: przy klapie otwartej na maksa panel pokaże
   **„180 %”**. Symulator celowo raportuje stopnie (żeby test odzwierciedlał
   produkt), więc tę niezgodność widać od razu.
2. **Panel czyta odpowiedź aż do zamknięcia gniazda.** `NonBlockingHttp` w
   `siec_nieblokujaca_LCD.h` wysyła `HTTP/1.0` + `Connection: close` i kończy
   dopiero na `recv() == 0`, a `CALKOWITY_TIMEOUT_MS` to 1500 ms. Dlatego ten
   firmware ma **własny mini-serwer HTTP**, który po wysłaniu `Content-Length`
   natychmiast zamyka gniazdo (jak `ESPAsyncWebServer` w centrali).
   Synchroniczny `WebServer` z rdzenia Arduino potrafi trzymać gniazdo do ~2 s —
   panel zgłaszałby timeout przy każdym odczycie.
3. **Bez czasu nie ma sezonu.** Przy `rtc_ok=false` panel pokazuje `--:--` i nie
   przełącza widoku Lato/Zima po miesiącu (dokładnie tak, jak centrala bez RTC).
   Stąd `rtc-sync` i automatyczne ustawianie czasu ze strony WWW.

---

## 7. Czego symulator NIE robi (świadome ograniczenia)

* **nie steruje niczym realnym** — brak przekaźników, PWM, serw, buzzera,
* **nie ma czujników** — brak DS18B20/DHT22/BMP280/MQ-2 (dane są symulowane),
* **brak Firebase / Telegrama / WebSocket-terminala / OTA** centrali,
* **tylko AP** (bez prób łączenia się z domowym routerem, więc `internet=false`
  i `rssi=0` — tak samo jak centrala w trybie AP),
* historia: 6 h próbek 60-sekundowych + 5 dni agregatów godzinnych
  (w centrali 24 h + 7 dni, ale tam jest 8 MB PSRAM),
* dynamika cieplna ~5× szybsza od rzeczywistej,
* nie odtwarza `normalizacji` NVS z prawdziwej centrali (namespace `piec`,
  te same skrócone klucze ≤15 znaków).

---

## 8. Testy rdzenia na PC (bez płytki)

Cała logika (ustawienia, walidacja, symulacja, JSON, CSV) jest w przenośnym C++
(`src/wspolne.h`, `src/ustawienia.h`, `src/symulacja.cpp`) — bez Arduino. Dzięki
temu da się ją sprawdzić na komputerze:

```bash
cd centrala-testowa-c3/testy_host
./uruchom_testy.sh
```

Skrypt kompiluje rdzeń `g++ -Wall -Wextra`, rozkręca **3 h wirtualnego czasu** i
weryfikuje to, co realnie widzi panel:

* `/api/status` — poprawny JSON + **komplet 31 kluczy**, które czyta panel,
  typy pól, zakresy `klapa` 0–180 / `syberka` 0–90,
* `/api/ustawienia-get` — komplet 21 kluczy z `POLA[]` + tryby,
* walidacja ustawień — oczekiwane przyjęcia/odmowy + **klucze NVS ≤ 15 znaków**,
* `/api/history` — 14 kolumn, rosnący `czas_unix`,
* `/api/history/long` — 35 kolumn.

Test po sieci (na prawdziwym C3, z telefonu/PC w `PiecCO_AP`):

```bash
python3 narzedzia/sprawdz_api.py 192.168.4.1 --czas --pelny
```

Sprawdza wszystkie endpointy, zapis ustawień, odmowę przy złej wartości, odmowę
serwa w trybie AUTO, wymuszenie pompy i liczbę kolumn CSV.

---

## 9. Pliki

```
centrala-testowa-c3/
├── platformio.ini          # env:esp32-c3, platforma przypięta
├── src/
│   ├── main.cpp            # warstwa Arduino: SoftAP, mini-serwer HTTP, NVS, zegar, konsola
│   ├── strona_www.h        # panel WWW (diagnostyka, wymuszanie awarii, czas)
│   ├── symulacja.cpp/.h    # silnik symulacji + JSON-y + CSV historii
│   ├── ustawienia.h        # walidacja 1:1 z ustawParametrPieca() + klucze NVS
│   └── wspolne.h           # Czas, formatowanie liczb, budowniczy JSON
├── testy_host/             # testy rdzenia na PC (g++ + Python)
└── narzedzia/
    └── sprawdz_api.py      # test kontraktu API po sieci (z PC/telefonu)
```

---

## 10. Kiedy wróci ESP32-S3

Ten projekt nic nie zmienia w prawdziwej centrali ani w panelu. Gdy S3 będzie
pod ręką: wgraj `Cenatrala Pieca.zip` (albo nowszy `firmware-patch/`) i podłącz
panel — nie trzeba nic przestawiać, bo AP i endpointy są te same. C3 może
zostać jako „centrala na biurku” do szybkich testów panelu bez palenia w piecu.

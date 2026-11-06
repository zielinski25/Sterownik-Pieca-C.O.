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
pio run -t upload          # domyslne env: esp32-c3, wgrywa po USB
pio device monitor         # konsola diagnostyczna, 115200
```

Są **trzy środowiska** (ten sam kod, zero różnic — firmware nie używa żadnych
GPIO, więc sam układ nie ma znaczenia):

| env | płytka | komenda |
|---|---|---|
| `esp32-c3` | **domyślne** — C3 SuperMini / DevKitM-1 / WROOM-02 | `pio run -t upload` |
| `esp32` | klasyczny ESP32 (WROOM-32, DevKit V1) | `pio run -e esp32 -t upload` |
| `esp32s3` | ESP32-S3 (DevKitC-1, S3 Mini) | `pio run -e esp32s3 -t upload` |

Po co więcej niż jedno: **symulator nie potrzebuje konkretnego układu**, a pod ręką
bywa to, co akurat jest. Który env wybrać, mówi jedna komenda — patrz 3b.
**Nie wgrywaj żadnego z nich na płytkę panelu LCD** — skasujesz firmware panelu.

Bez PlatformIO (samo esptool):

```bash
pio run                    # zbuduje .pio/build/esp32-c3/firmware.bin
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 firmware.bin
```

> **Status (2026-11-06):** firmware wgrany i uruchomiony na **klasycznym ESP32**
> (`pio run -e esp32 -t upload`, COM5/CH340). Build: RAM 20,2 %, flash 60,2 %.

Po starcie w konsoli zobaczysz m.in.:

```
[AP] wstalo  SSID="PiecCO_AP"  IP=192.168.4.1  kanal=6
[HTTP] nasluchuje na 192.168.4.1:80
[CZAS] brak zapisanego czasu — panel pokaze "--:--".
```

---

## 3a. Upload: „This chip is ESP32 not ESP32-C3. Wrong --chip argument?”

**Skrót, jeśli `chip_id` mówi `Chip is ESP32-D0WD...`:** build zrobiony dla C3 nie
poleci na klasycznym ESP32 (inny rdzeń — RISC-V vs Xtensa), trzeba **przebudować**
pod `env:esp32`:

```powershell
pio run -e esp32 -t upload
pio device monitor
```

Nie da się tego obejść przez `esptool.py --chip esp32 write_flash` na pliku z C3 —
to inny zestaw instrukcji, płytka się nie uruchomi. Musi to być build z `-e esp32`.

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

## 3b. „Mam takie ESP” — co to za układ i który env

Najpewniejsze rozstrzygnięcie to nie zdjęcie, a odpowiedź samego układu:

```powershell
pio pkg exec --package platformio/tool-esptoolpy -- esptool.py --port COM5 chip_id
```

| co pisze `chip_id` | co to jest | env | polecenie |
|---|---|---|---|
| `Chip is ESP32-C3 (QFN32)` | C3 SuperMini / C3-MINI-1 — mała płytka ~22×18 mm, przyciski BOOT+RESET obok siebie | `esp32-c3` | `pio run -t upload` |
| `Chip is ESP32-D0WD-V3` (też `D0WDQ6`, `ESP32-D0WD`) | **klasyczny ESP32** — WROOM-32, płytka ~55×28 mm, 30 pinów, micro-USB | `esp32` | `pio run -e esp32 -t upload` |
| `Chip is ESP32-S3` | S3 — zwykle USB-C + drugie gniazdo UART, napis `S3-WROOM-1` | `esp32s3` | `pio run -e esp32s3 -t upload` |

Czyli: **`A fatal error occurred: This chip is ESP32 not ESP32-C3` nie jest błędem
kodu ani złym `platformio.ini`** — to esptool mówi, jaki układ naprawdę wisi na
porcie COM. Jeśli to Twoja płytka „centrali”, po prostu użyj env `esp32`: ten sam
firmware, ten sam kontrakt HTTP, panel tego nie odróżni. Szukanie C3 ma sens
tylko wtedy, gdy *koniecznie* chcesz C3.

Skrót: **jedna płytka pod USB → jeden port COM → `-e` wynika wprost z `chip_id`.**

---

## 3c. Build: `can't open file '...\packages\tool-esptoolpy\esptool.py'`

Objaw (Windows):

```
Compiling .pio\build\esp32-c3\src\symulacja.cpp.o
Building .pio\build\esp32-c3\bootloader.bin
C:\Users\...\.platformio\python3\python.exe: can't open file
'C:\Users\...\.platformio\packages\tool-esptoolpy\esptool.py': [Errno 2] No such file or directory
*** [.pio\build\esp32-c3\bootloader.bin] Error 2
```

Co się stało: **kod kompiluje się dobrze** (widać `.o`), a wysypuje się dopiero
krok `bootloader.bin` — PlatformIO woła wtedy `esptool.py` z pakietu
`tool-esptoolpy`. Tego pliku nie ma na dysku: pakiet jest **niedociągnięty**
(przerwany download/przerwany `pio run`), **uszkodzony**, albo **wersja pakietu
nie pasuje** do przypiętej platformy (`espressif32 @ 6.9.0` → `tool-esptoolpy
~1.40501.0`, wtedy plik nazywa się dokładnie `esptool.py`).

Charakterystyczne: przy **pierwszej** próbie uploadu esptool jeszcze działał
(padło dopiero `This chip is ESP32 not ESP32-C3`), a teraz pliku nie ma — czyli
pakiet **zniknął pomiędzy uruchomieniami** (antywirus, czyszczenie cache,
przerwane pobieranie), a nie „nigdy go nie było”.

Naprawa (PowerShell, 3 komendy, ~30 s):

```powershell
pio pkg list -g
dir C:\Users\bakoma\.platformio\packages\tool-esptoolpy
```

Jeśli katalog nie istnieje, jest pusty albo nie ma w nim `esptool.py`:

```powershell
Remove-Item -Recurse -Force $env:USERPROFILE\.platformio\packages\tool-esptoolpy
pio pkg install -g --tool "platformio/tool-esptoolpy@~1.40501.0"
pio run -t upload
```

> **`--tool` jest obowiązkowe.** Bez niego (`pio pkg install -g -p platformio/tool-esptoolpy`)
> PlatformIO szuka tego pakietu w rejestrze *bibliotek* i kończy się
> `UnknownPackageError: Could not find the package with 'platformio/tool-esptoolpy' requirements`
> — mimo że pakiet istnieje i `pio run` sam go zainstaluje.

> Jeśli błąd **wraca** po kilku buildach — to nie jest już uszkodzony pakiet, a
> konflikt wersji z drugą platformą `espressif32` na tym PC. Patrz **3d**.

Uwagi:

* **Nie usuwaj całego** `.platformio\packages` — tam siedzą też toolchainy
  (kilkaset MB do pobrania ponownie). Kasujemy tylko katalog `tool-esptoolpy`.
* Jeśli `pio pkg install` znów kończy się błędem sieci/proxy, powtórz — pakiet
  jest mały (~300 kB).
* Antywirus potrafi „kwarantannować” `.py` w `packages`. Jeśli błąd wraca, dodaj
  `C:\Users\bakoma\.platformio` do wykluczeń.
* Ten błąd **nie ma nic wspólnego** z kodem symulatora ani z tym, czy płytka jest
  C3/ESP32 — to wyłącznie narzędzie uploadu na Twoim PC.

---

## 3d. Dwie platformy `espressif32` na jednym PC → walka o `tool-esptoolpy`

Objaw: build przechodzi (`.o` się kompilują), ale pada generowanie bootloadera:

```
Building .pio\build\esp32-c3\bootloader.bin
...python.exe: can't open file '...\.platformio\packages\tool-esptoolpy\esptool.py': [Errno 2]
```

Diagnoza — `pio pkg list -g` na takiej maszynie pokazuje **dwa** wpisy:

```
tool-esptoolpy @ 5.2.0        (required: file://C:\...\.platformio\tools\tool-esptoolpy)
tool-esptoolpy @ 1.40501.0    (required: platformio/tool-esptoolpy)
```

Skąd: obok tego projektu jest drugi — prawdziwa centrala na **pioarduino**
(`espressif32 @ 55.3.38`, Arduino core 3.3.8, esptool **5.2.0**). Ten projekt
używa `espressif32 @ 6.9.0`, gdzie `platform.json` wymaga `tool-esptoolpy
~1.40501.0` (esptool **4.5.1**). PlatformIO trzyma **jedną wersję pakietu na
nazwę**: `%USERPROFILE%\.platformio\packages\tool-esptoolpy`. Build C3 trafia
w moment, gdy katalog jest właśnie podmieniany → pliku nie ma. To nie jest błąd
kodu ani `platformio.ini`; to dwie platformy walczące o jeden katalog.

### Naprawa A — szybka (do testu)

```powershell
pio pkg install -g --tool "platformio/tool-esptoolpy@~1.40501.0"
pio run -t upload
```

> `pio pkg install` **wymaga `--tool`** — bez tego PlatformIO szuka
> `platformio/tool-esptoolpy` w rejestrze *bibliotek* i odpowiada
> `UnknownPackageError: Could not find the package...`. To nie znaczy, że
> pakietu nie ma.

Minus: przy następnym buildzie prawdziwej centrali (pioarduino) PlatformIO
wstawi 5.2.0 i przy kolejnym buildzie C3 znowu podmieni — czyli co drugi build
może trafić w podmianę.

### Naprawa B — trwała (osobny core dir)

Daj temu projektowi własny katalog rdzenia, żeby w ogóle nie dotykał
wspólnego `packages\`:

```powershell
$env:PLATFORMIO_CORE_DIR="$env:USERPROFILE\.platformio-centrala-c3"
pio run -t upload
```

albo gotowy wrapper (robi to samo, wygodny w VS Code):

```powershell
.\narzedzia\pio-c3.cmd run -t upload
.\narzedzia\pio-c3.cmd device monitor
```

Cena: pierwszy build pobiera platformę + toolchain (~350 MB, 10–20 min) do
`%USERPROFILE%\.platformio-centrala-c3`. Zysk: pełna niezależność od projektu
pioarduino — koniec podmian i „raz działa, raz nie”.

---

## 3e. Upload: `PermissionError(13, 'Odmowa dostępu')` / `Cannot configure port`

```
Connecting.............................
A serial exception error occurred: Cannot configure port, something went wrong.
Original message: PermissionError(13, 'Odmowa dostępu.', None, 5)
```

To **nie** jest błąd firmware'u ani zły `env` — build się udał, esptool nie może
otworzyć/ustawić COM5, bo **port jest zajęty przez inny proces** (albo sterownik
CH340 został w złym stanie po poprzedniej, przerwanej próbie).

Kolejność (od najczęstszej przyczyny):

1. **Zamknij monitor szeregowy.** `pio device monitor` (albo zakładka Serial
   Monitor w VS Code) trzyma port nawet gdy nic nie wypisuje. Ctrl+C w oknie
   monitora, ewentualnie zamknij terminal.

2. **Sprawdź, kto jeszcze żyje** (stare `python.exe` po przerwanym uploadzie,
   Arduino IDE, PuTTY):

   ```powershell
   Get-Process | Where-Object { $_.ProcessName -match 'python|platformio|arduino|putty|pio' } |
       Select-Object Id, ProcessName, Path
   ```

   Ubij tylko te z `.platformio` w ścieżce:

   ```powershell
   Get-Process python* -ErrorAction SilentlyContinue |
       Where-Object { $_.Path -like "*platformio*" } | Stop-Process -Force
   ```

3. **Odłącz i podłącz kabel USB** (liczy się pełne odłączenie, nie reset
   przyciskiem). Po nieudanym `Connecting...` CH340/CP210x potrafi zostać
   zablokowany aż do przełączenia zasilania. W Menedżerze urządzeń można też
   wyłączyć i włączyć urządzenie `USB-SERIAL CH340 (COM5)`.

4. **Ponów upload:**

   ```powershell
   pio run -e esp32 -t upload --upload-port COM5
   ```

5. **Nadal blokada?** Zejdź z prędkością — część klonów CH340 i długie kable nie
   wyrabiają 460800:
   ```ini
   ; platformio.ini, w [env:...]
   upload_speed = 115200
   ```

Inne typowe przyczyny „Odmowa dostępu”:

* dwie kompilacje/uploady naraz (przycisk **Upload** w VS Code **i** `pio run`
  w terminalu) — port bierze ten, kto pierwszy,
* wirtualny port COM innego urządzenia o tym numerze (kolejność COM po
  restarcie) — porównaj `pio device list` z tym, co pokazuje Menedżer urządzeń,
* zasilanie z portu USB huba bez zasilacza — płytka resetuje się w trakcie
  `Connecting...`.

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

**Panel nie wymaga żadnej konfiguracji** — ma to zaszyte w `main.cpp` panelu:

| parametr panelu | wartość | czy zgadza się z symulatorem |
|---|---|---|
| `WIFI_SSID` | `PiecCO_AP` | tak |
| `WIFI_PASS` | `KociolCO2026` | tak |
| `CENTRALA_HOST` | `192.168.4.1:80` | tak (SoftAP symulatora) |
| IP panelu | statyczne `192.168.4.50` | tak, DHCP symulatora tego nie zajmuje |

Czyli: **włącz panel, gdy symulator już stoi** — panel sam wejdzie do `PiecCO_AP`,
dostanie `.50` i zacznie pytać `192.168.4.1`. Jeśli po ~30 s dalej wisi na
„Łączenie…”, odłącz panel od zasilania na 5 s: trzyma w pamięci RTC **BSSID i kanał
poprzedniej centrali** (prawdziwej) i po zimnym starcie zrobi pełne skanowanie.

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

### 5a. Panel „nic nie odbiera” — rozdziel WiFi od HTTP w 30 sekund

W konsoli Serial symulatora wpisz:

```
diag
```

i czytaj **dwie** liczby: `stacje=` (ilu klientów w AP) i `zapytania=` (ile zadań
HTTP przyszło od startu).

| co widzisz | co to znaczy | co robić |
|---|---|---|
| `stacje=0`, `zapytania=0` | panel **nie wszedł do WiFi** — do serwera nic nie dotarło | punkt A |
| `stacje=1`, `zapytania=0` | panel **jest w sieci, ale nic nie wysyła** (albo wysyła coś nieczytelnego) | punkt B |
| `zapytania` rośnie | serwer odpowiada, problem jest po stronie ekranu panelu | punkt C |

**A. Panel nie wchodzi do AP**
* odłącz panel od zasilania na 5 s (trzyma w pamięci RTC **BSSID i kanał
  poprzedniej** centrali — po zimnym starcie robi pełne skanowanie),
* sprawdź, czy panel w ogóle żyje: czy na ekranie jest cokolwiek (logo, „Łączenie…”),
  czy ekran jest biały/czarny,
* sprawdź, czy hasło w panelu to `KociolCO2026` (a nie inne) — panel ma je zaszyte
  na sztywno w `main.cpp`,
* kanał: panel po zimnym starcie sam znajdzie kanał 6, ale jeśli siedzi w pamięci
  RTC inny — patrz pierwszy punkt.

**B. Panel w sieci, ale brak zadań**
W konsoli pojawi się `[AP] stacji w sieci: 1`, a przy ewentualnych próbach
`[HTTP] polaczenie bez rozpoznanego zadania od 192.168.4.50`. To znaczy, że
gniazdo działa, a treść żądania nie przechodzi parsera. Z panelu zrób zrzut
Seriala (panel podłączony po USB, `pio device monitor -b 115200`) i sprawdź, na
czym stoi `httpPobierz()`.

**C. Serwer odpowiada, ekran nie**
`zapytania` rośnie, a panel stoi → problem w panelu (parsowanie JSON, rysowanie),
nie w symulatorze. Warto wtedy porównać `/api/status` z symulatora
(`python narzedzia/sprawdz_api.py`) — klucze muszą się zgadzać co do nazwy.

**Test z PC (bez panelu):** wejdź laptopem do `PiecCO_AP` i:
```powershell
ping 192.168.4.50        # czy panel jest w sieci pod swoim statycznym IP
curl http://192.168.4.1/api/status
```
Jeśli `ping` do `.50` nie odpowiada, panel **nie jest** w sieci — punkt A.

> **Uwaga — najczarniejszy scenariusz.** Jeśli TFT jest fizycznie podłączony do
> **tej samej płytki, którą wgrałeś** (tej na COM5), to znaczy, że firmware panelu
> został nadpisany symulatorem: ekran nie ma prawa nic pokazać, bo nie ma już kodu
> panelu. Wtedy: symulator idzie na **inną** płytkę, a panel odtwarzasz z
> `LCD Centrali.zip`. Sprawdź to, zanim zaczniesz szukać WiFi.

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
    ├── sprawdz_api.py      # test kontraktu API po sieci (z PC/telefonu)
    ├── znajdz_port.ps1     # ktory port COM to ktory uklad (chip_id)
    └── pio-c3.cmd          # PlatformIO z osobnym core dir (patrz 3d)
```

---

## 10. Kiedy wróci ESP32-S3

Ten projekt nic nie zmienia w prawdziwej centrali ani w panelu. Gdy S3 będzie
pod ręką: wgraj `Cenatrala Pieca.zip` (albo nowszy `firmware-patch/`) i podłącz
panel — nie trzeba nic przestawiać, bo AP i endpointy są te same. C3 może
zostać jako „centrala na biurku” do szybkich testów panelu bez palenia w piecu.

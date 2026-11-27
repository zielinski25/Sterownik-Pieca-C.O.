# Firmware centrali 3.58.22 — DHT22: kod biblioteki i surowa wartość w zwykłym logu **i w telemetrii Firebase**

Data: 2026-11-27 · następna po **3.58.21** (`firmware-patch/v3.58.21-zew2-odniesienie-ds/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 634 linie, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.21: **9 hunków, ok. 45 linii kodu**. **Tylko diagnostyka** — działanie czujników, filtr Zew 2,
dekoder klona, odniesienie DS18B20, HTML/JS, `/api/status`, dotychczasowe pola Firebase, LCD: bajt w bajt jak
w 3.58.21 (test C: po wycięciu 5 oznaczonych wstawek `[v3.58.22 …]` plik = 3.58.21).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Dlaczego
27.11 07:46 wgrana 3.58.21 (OTA). Eksport Firebase (`…-27-export.json`) i log z Telegrama (`piec_log (2).txt`,
16 708 linii od 15.11) pokazują:
- noc 00:00–06:45 UTC: Zew 1 0,8–2,4 °C, **Zew 2 BRAK przez całą noc**, RH zamrożona na 83,4 % od 26.11 14:51
  (wiek 57 216 s);
- po restarcie (boot 15, `po_ota=1`) przez 15 min: `wilgotnosc=0.0%` w `tag=STATUS`, w Firebase RH „nigdy”
  (`age=65535`), flaga `q` = INVALID → **próby odczytu DHT były i wszystkie odpadły**;
- w całym logu **ani jednej linii `DHT22`** — wszystkie idą przez `DLOGF(DC_SENSOR, …)`, a archiwum diag jest
  domyślnie wyłączone (`dArcMask = 0`, w logu widać tylko `diag remote …` z 18.11). Włącza się je wyłącznie
  z Terminala WWW; użytkownik jest poza domem i ma tylko Telegram.

Bez tej linii nie da się rozstrzygnąć, **dlaczego** ramki odpadają: inne kodowanie ujemnych (dekoder 3.58.20 nie
trafia), czy błąd transmisji na zimnie (`BAD_DATA`/`CHECKSUM`/`TIMEOUT` z biblioteki `dhtESP32-rmt` — sprawdzone
w źródle biblioteki: brak kontroli zakresu, okna impulsów 55–145 µs, próg bitu 110 µs, start 130–180 µs).

## Zmiana
W `wilgotnosc()`, po zliczeniu próby, linia przez `zapiszLog()` (zwykły log `log_a/log_b`, zawsze archiwowany,
przychodzi po `/logi`):
```
tag=DHT22 lvl=INFO proba=3 kod=0 surowa=-3275.3 t=-1.5 rh=84.2 ramka=ok decyzja=0 u2=3 ds=1.6
```
`kod` — kod biblioteki (0 OK, 1 TOO_SOON, 2 DRIVER, 3 TIMEOUT, 4 NACK, 5 BAD_DATA, 6 CHECKSUM, 7 UNDERFLOW,
8 OVERFLOW); `surowa` — float z biblioteki **przed** dekoderem klona; `t` — po dekoderze; `ramka` — ok/zła;
`decyzja` — 0 przyjęty, 1 do weryfikacji, 2 potwierdzony, 3 odrzucony, 4 zła ramka; `ds` — DS18B20 (`nan` bez DS).

Rytm (stałe przy `DHT_PIN`): każda z pierwszych **30** prób po starcie (~1 h), potem **co 10.** (~20 min) oraz
**zmiana ok ↔ zła** od razu, najwyżej raz na 10 min. Przy 5-minutowym `tag=STATUS` przyrost logu jest pomijalny.

### To samo w telemetrii minutowej (Firebase) — pomysł użytkownika (27.11)
Każdy rekord `/piec/telemetry/<rok>/<mies>/<dzien>/<id>` dostaje dwa pola o **ostatnim** odczycie DHT22:
```
..., "t_zewn_2": -1.5, "dht_kod": 0, "dht_surowa": -3275.3, "age": [...]
```
`dht_kod` — kod biblioteki (jak `kod=` wyżej); `dht_surowa` — float z biblioteki **przed** dekoderem klona, tylko gdy
`dht_kod = 0` (przy błędzie biblioteka nic nie oddaje). Przed pierwszą próbą po starcie pól nie ma.
Wystarczy więc eksport RTDB albo podgląd w konsoli Firebase — bez `/logi`.

Jak to jest niesione bez zmiany ABI próbki (`TelemetrySample` = 80 B, `static_assert` zostają): surowa ×10 w dotąd
zawsze zerowym `int16_t tempDiagReserved` (offset 78; zakres ±3276,7 pokrywa cały 16-bit obraz klona), kod w wolnych
bitach 11..14 `simulatedBits` (kanały zajmują 0..10), bit 15 = „dane DHT obecne” (stare próbki z ringu: 0 → pól nie
ma). Do JSON `"sim"` idzie teraz `simulatedBits & 0x07FF` — czyli dokładnie to, co dotąd. `wilgotnosc()` zapisuje
`g_dhtOstatniKod` / `g_dhtOstatniaSurowaX10` przy każdej próbie; `telemetryBuildSnapshot()` kopiuje je do próbki.
Aplikacja Android (`RtdbClient.kt`), `Piec.html` i skrypty ignorują nieznane klucze — sprawdzone w źródłach.

## Testy na PC (`tests/uruchom_testy.sh`, wynik w `WYNIKI/testy-pc.txt`)
- **A** blok `ZEW2-FILTR` (nietknięty): `[1]`–`[18]` z 3.58.10/20/21 — **125 sprawdzeń, 0 błędów** (także ASan+UBSan).
- **B** prawdziwa `wilgotnosc()` z atrapami (`zapiszLog()` → bufor testu): `[1]`–`[14]` regresja + w `[1]` próby 1 i 2
  mają linię `tag=DHT22 … kod=0 surowa=2.0 t=2.0 rh=90.0 ramka=ok`, `[15]` po 30. próbie tylko co 10., timeout →
  od razu `kod=2 … ramka=zla decyzja=4`, powtórne złe bez spamu, powrót do ok w < 10 min bez linii, po 10 min linia
  `surowa=-3276.3 t=-0.5` — **85 sprawdzeń, 0 błędów**.
- **B2** prawdziwa telemetria z pliku (`wytnij_telemetria.py`: `TelemetrySample` + `static_assert`y,
  `telemetryAppendPathForSample()`, fragment `telemetryBuildSnapshot()`): `[1]` stara próbka → JSON bajt w bajt jak
  dotąd (ścieżka `2026/11/27/<id>`, `a`, `sim:0`, bez `dht_*`), `[2]` kod 0 / surowa −3275,3 / t −1,5 →
  `"t_zewn_2":-1.5,"dht_kod":0,"dht_surowa":-3275.3`, `q` bez znacznika, `sim` nadal 0, `[3]` kod 6 → `dht_kod:6` bez
  surowej, `sim` = maska kanałów 0x0555 mimo bitów DHT w strukturze, `[4]` granice 80,0 / 0,0 / −0,1 / 409,5 / ±3276,7,
  kod 15 i obcięcie >15, `[5]` przed pierwszą próbą brak pól, `[6]` 80 B, offsety 52/78 — **25 sprawdzeń, 0 błędów**
  (także ASan+UBSan).
- **C** znaczniki 3.58.22, linia po `++dhtProby` i przed decyzją o RH, filtr/dekoder/DS bez zmian, `g_dhtOstatni*`
  w `wilgotnosc()`, pola `dht_kod`/`dht_surowa` i maska `sim` w JSON, `static_assert` 80 B / offset 78, plik = 3.58.21 +
  patch (SHA), HTML, blok `ZEW2-FILTR`; po wycięciu 5 oznaczonych wstawek kod przed i za `wilgotnosc()` identyczny
  z 3.58.21.
- **D** pełne testy 3.58.21 (i przez nią 3.58.20…3.58.14) na tym pliku (do harnessów B 3.58.21/3.58.20/3.58.10
  dopisana atrapa `zapiszLog()`, stałe `DHT_LOG_*` i zmienne `g_dhtOstatni*`; test „kod za `wilgotnosc()` identyczny
  jak w 3.58.19/20” z definicji nie przechodzi — telemetria się zmieniła — i jest na liście znanych; logika testów
  bez zmian).

## Pliki
`src/main_centrala.cpp` · `main_centrala-v3.58.21-do-v3.58.22.patch` · `tests/` (`uruchom_testy.sh`,
`wytnij_zew2_filtr.py`, `wytnij_wilgotnosc.py`, `wytnij_telemetria.py`, `test_*.cpp`) · `WYNIKI/zakres-zmian.txt` ·
`WYNIKI/testy-pc.txt` · `SHA256SUMS.txt` · `CZYTAJ-NAJPIERW.txt`.

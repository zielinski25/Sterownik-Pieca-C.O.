# Firmware centrali 3.58.22 — DHT22: wynik każdego odczytu w zwykłym logu (bez `diag archive`)

Data: 2026-11-27 · następna po **3.58.21** (`firmware-patch/v3.58.21-zew2-odniesienie-ds/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 594 linie, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.21: **5 hunków, ok. 30 linii kodu**. **Tylko diagnostyka** — działanie czujników, filtr Zew 2,
dekoder klona, odniesienie DS18B20, HTML/JS, `/api/status`, Firebase, LCD: bajt w bajt jak w 3.58.21 (test C).

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

## Testy na PC (`tests/uruchom_testy.sh`, wynik w `WYNIKI/testy-pc.txt`)
- **A** blok `ZEW2-FILTR` (nietknięty): `[1]`–`[18]` z 3.58.10/20/21 — **125 sprawdzeń, 0 błędów** (także ASan+UBSan).
- **B** prawdziwa `wilgotnosc()` z atrapami (`zapiszLog()` → bufor testu): `[1]`–`[14]` regresja + w `[1]` próby 1 i 2
  mają linię `tag=DHT22 … kod=0 surowa=2.0 t=2.0 rh=90.0 ramka=ok`, `[15]` po 30. próbie tylko co 10., timeout →
  od razu `kod=2 … ramka=zla decyzja=4`, powtórne złe bez spamu, powrót do ok w < 10 min bez linii, po 10 min linia
  `surowa=-3276.3 t=-0.5` — **85 sprawdzeń, 0 błędów**.
- **C** znaczniki 3.58.22, linia po `++dhtProby` i przed decyzją o RH, filtr/dekoder/DS bez zmian, plik = 3.58.21 +
  patch (SHA), HTML, blok `ZEW2-FILTR`, kod przed i za `wilgotnosc()` identyczne z 3.58.21.
- **D** pełne testy 3.58.21 (i przez nią 3.58.20…3.58.14) na tym pliku (do harnessów B 3.58.21/3.58.20/3.58.10
  dopisana atrapa `zapiszLog()` i stałe `DHT_LOG_*`; logika testów bez zmian).

## Pliki
`src/main_centrala.cpp` · `main_centrala-v3.58.21-do-v3.58.22.patch` · `tests/` · `WYNIKI/zakres-zmian.txt` ·
`WYNIKI/testy-pc.txt` · `SHA256SUMS.txt` · `CZYTAJ-NAJPIERW.txt`.

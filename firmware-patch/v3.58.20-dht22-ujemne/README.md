# Firmware centrali 3.58.20 — Zew 2 (DHT22) poniżej 0 °C: klon koduje ujemne w U2

Data: 2026-11-27 · następna po **3.58.19** (`firmware-patch/v3.58.19-plynny-deszcz/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 483 linie, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.19: **6 hunków, ok. 36 linii kodu** (reszta to komentarze i wpis w historii), wyłącznie C++
w bloku `ZEW2-FILTR` i w `wilgotnosc()`. HTML/JS strony WWW, `/api/status`, Firebase, telemetria minutowa —
bajt w bajt / pole w pole jak w 3.58.19 (sprawdzane testem). **Panel LCD bez zmian** (v1.5.18).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Zgłoszenie
26.11: przy pierwszych przymrozkach kafelek **Zew 2** (DHT22) na Androidzie pokazuje **BRAK DANYCH**, a Zew 1
(DS18B20) 1,9 °C.

## Co pokazał eksport Firebase z 26.11 (`akwarium-367be-default-rtdb-26-export.json`, 1271 próbek co minutę, UTC)

| czas (UTC) | Zew 1 (`a[0]/10`) | Zew 2 (`t_zewn_2`) | RH `a[8]` / `age[8]` |
|---|---|---|---|
| 00:00 | 4,9 | 0,8 | 88,8 / świeża |
| 01:45–02:20 | 2,6–2,9 | 0,0 ↔ 0,3 | świeża |
| 03:05–03:13 | 2,3–2,7 | **0,0** | age 14 → 254 (ramki co drugą przyjmowane) |
| 03:14–03:33 | 2,1–3,3 | **BRAK** | 89,6 zamrożone, age 314 → 1454 |
| 03:34–04:47 | 3,1–4,3 | 0,0 ↔ 0,2 (przerwa 03:38) | — |
| 04:48–06:11 | 3,1–3,6 | **BRAK** | zamrożone |
| 06:22–14:49 | 3,1–5,2 | 0,0 → 1,8 → 0,0 | świeża |
| 14:50–14:55 | 2,8–3,2 | BRAK / 0 / BRAK | — |
| **14:56–21:15** | 2,2–2,9 | **BRAK na stałe** | 83,4 zamrożone, age → **23 076 s (6,4 h)** |

Wnioski:
1. Przez cały dzień `t_zewn_2` **ani razu nie jest ujemne** (778 wartości ≥ 0, 493 null) — a Zew 2 chodzi u Ciebie
   ok. 2,5–3 °C niżej niż Zew 1, więc w nocy i po 14:56 realnie było ok. −0,5…−1 °C.
2. Gdy Zew 2 znika, **znika też wilgotność z tej samej ramki** (`age[8]` rośnie godzinami, flaga `q` +0x30000).
   Odrzucenie przez filtr skoków zostawia RH jako przyjętą — więc to nie filtr, tylko **cała ramka odpada
   w `zew2RamkaPoprawna()`** (temperatura poza −40..80 albo błąd biblioteki).
3. Przy temperaturze oscylującej wokół zera ramki **0,0 przechodzą, a −0,1 nie** (naprzemienne 0 / BRAK co ~2 min);
   od −0,x na stałe BRAK. Dokładnie tak zachowuje się DHT22 (klon AM2302), który koduje ujemne
   w **kodzie uzupełnień do dwóch** (−0,1 = `0xFFFF`), a nie — jak karta katalogowa — znak-moduł (−0,1 = `0x8001`).

Biblioteka `dhtESP32-rmt` dekoduje wg karty: `t = ((d2 & 0x7F)·256 + d3)/10`, minus gdy `d2 & 0x80`.
Dla ramki U2: −0,1 → **−3276,7**, −5,0 → −3271,8, −40,0 → −3236,8 (zawsze −3276,8 + |t|) → poza −40..80 →
`ZEW2_ZLA_RAMKA` → po 255 s `t_zewn_2 = null` → BRAK DANYCH. Do potwierdzenia na płytce:
`diag archive cat SENSOR on` → log `DHT22 Zew2 zdarzenie=zla_ramka t=-3276.x rh=8x.x` (3.58.19) albo
`zdarzenie=u2 surowa=-3276.x t=-0.x` (3.58.20).

## Co mówią inni o tym problemie (przegląd zgłoszeń, 2012–2026)
To znany, wielokrotnie opisany problem z klonami DHT22/AM2302 — ten sam objaw „−3276,x °C poniżej zera” zgłaszano
w Tasmocie, ESPHome, ESPEasy, bibliotekach Adafruit (Arduino i CircuitPython), DHTNew Roba Tillaarta i na Arduino
Forum (2012, 2017, 2024). Wnioski z tych zgłoszeń:

| kodowanie ujemnych | przykład −0,1 °C (bajty 2–3) | co oddaje nasza biblioteka | kto tak robi |
|---|---|---|---|
| **znak-moduł** (karta katalogowa) | `80 01` | −0,1 ✔ | oryginalne Aosong/ASAIR — zrzut z prawdziwego czujnika w DHTNew #100: `80 02` = −0,2 |
| **U2 16-bit** | `FF FF` | **−3276,7** ✘ | najczęstszy klon (AliExpress/eBay); Tasmota #14164, ESPHome #1744, Adafruit DHT #177/#221, DHTNew #100, Arduino Forum 1213646 |
| **U2 12-bit** | `0F FF` | **+409,5** ✘ | rzadszy klon „świeżo z Chin”; ESPHome #5322 (log: `0F F8` → 408,8 °C zamiast −0,8), poprawione w ESPHome PR #6074 |

Jak to rozwiązali inni:
- **Tasmota** (`xsns_06_dht.ino`, 12.2021): bierze 16 bitów jako `int16_t` (U2), a znak-moduł rozpoznaje po górnej
  ćwiartce bajtu `0x8_` → `-(0xFFF & raw)`.
- **ESPHome** (`decode_dht22_temperature()`, 01.2024 + poprawka 11.2026): bit 15 = 1 i bit 14 = 0 → znak-moduł;
  bit 15 = 1 i bit 14 = 1 → U2 16-bit; bit 15 = 0 i bit 11 = 1 → U2 12-bit (rozszerzenie znaku `| 0xF000`).
- **DHTNew** (Tillaart, 0.4.22, 11.2024): bit 15 = 1 → jeśli bit 14 = 0 znak-moduł, inaczej `int16_t`.
- **Adafruit DHT-sensor-library** #221 (12.2024): propozycja „zawsze `int16_t`” — ale to psuje oryginały
  (−0,2 = `80 02` czytane jako −3276,6); nie wdrożono.
- Dedykowane biblioteki: `Zapivara/DHT22_Clone_ESP32` (02.2026, RMT jak u nas, autodetekcja po `bajt2 == 0xFF`),
  `DMohammed/DHT22_Universal`.
- Proste obejścia użytkowników: `if (t < -50) t = -3276.8 - t;` (Arduino Forum 2024) — to w istocie nasza formuła.

Nasza sytuacja: biblioteka `dhtESP32-rmt` zwraca już `float`, bez surowych bajtów, więc nie da się patrzeć na bity —
ale obraz każdego z błędnych kodowań po stronie `float` jest jednoznaczny i leży daleko poza −40..80 °C, więc
przeliczenie po wartości jest równoważne i równie bezpieczne. **Z danych Firebase nie da się odróżnić klona 16-bit
od 12-bit** (oba dają „złą ramkę” i BRAK DANYCH), dlatego dekoder obsługuje oba, a log WARN mówi, który to był.

## Zmiana
`zew2DekodujU2(float*, uint8_t* wariant)` w bloku `ZEW2-FILTR` (testowalnym na PC):
- `t` w **−3276,75..−3236,75** (obraz U2 16-bit dla −0,1..−40,0 °C) → `t = −lround((t + 3276,8)·10)/10`, wariant 16;
- `t` w **369,55..409,55** (obraz U2 12-bit dla −0,1..−40,0 °C) → `t = −lround((409,6 − t)·10)/10`, wariant 12;
- inaczej bez zmian. Prawdziwy DHT22 nigdy nie oddaje wartości z tych przedziałów (jego zakres to −40..80 °C).

Wywołanie w `wilgotnosc()` zaraz po `read_dht()`, tylko przy `DHT_OK`, **przed** `zew2RamkaPoprawna()` (zakres −40..80
sprawdzany na przeliczonej wartości) i przed filtrem skoków (odkodowana wartość przechodzi tę samą weryfikację co
każda inna). Pierwsze wykrycie: jedna linia WARN `DHT22 Zew2 zdarzenie=u2 wariant=16 surowa=-3276.7 t=-0.1 msg="klon
koduje ujemne w U2, przeliczam"`; w linii INFO `DHT22 diagnostyka` nowe pole `u2=<licznik>`.

Wartości dodatnie 0..80, prawdziwe ujemne ze znakiem w bicie 15, 0,0/−0,0, NaN, U2 poniżej −40 °C i śmieci
(−3276,8, +3276,7, +409,6, RH > 100) — bez zmian w zachowaniu. Skutek uboczny na plus: **wilgotność** z DHT22 przy
mrozie przestaje zamierać (do 3.58.19 odpadała razem z temperaturą).

## Testy na PC (`tests/uruchom_testy.sh`, wynik w `WYNIKI/testy-pc.txt`)
- **A** blok `ZEW2-FILTR` wycięty z pliku: 13 scenariuszy z 3.58.10 (regresja) + `[14]` dekoder na granicach obu
  wariantów (w tym `0x0FCC` → −5,2 z testu ESPHome PR #22615), `[15]` wszystkie 400 wartości −0,1..−40,0 °C
  w **trzech** kodowaniach przez dokładną formułę biblioteki + 801 dodatnich 0,0..80,0 nietkniętych,
  `[16]` scenariusz z 26.11 — **109 sprawdzeń, 0 błędów** (także ASan+UBSan).
- **B** prawdziwa `wilgotnosc()` z atrapami: 9 scenariuszy z 3.58.10 + `[10]` klon U2 16-bit poniżej zera
  (Zew 2 −0,1/−0,3/−1,2, RH ACCEPTED, WARN raz z `wariant=16`, `jsonTempDiag() = "-0.1"`), `[11]` śmieci nadal
  INVALID, `[12]` klon U2 12-bit (+408,8 → −0,8, +407,9 → −1,7, `u2=6`) — **66 sprawdzeń, 0 błędów**.
- **C** znaczniki 3.58.20, kolejność `read_dht → zew2DekodujU2 → zew2RamkaPoprawna`, `zew2RamkaPoprawna()` bez zmian,
  plik = 3.58.19 + patch (SHA), HTML i kod poza blokiem/`wilgotnosc()` identyczny z 3.58.19.
- **D** pełne testy paczki 3.58.19 (i przez nią 3.58.18…3.58.14) na tym pliku.

## Pliki
`src/main_centrala.cpp` · `main_centrala-v3.58.19-do-v3.58.20.patch` · `tests/` (`uruchom_testy.sh`,
`wytnij_zew2_filtr.py`, `wytnij_wilgotnosc.py`, `test_zew2_filtr.cpp`, `test_wilgotnosc.cpp`) ·
`WYNIKI/zakres-zmian.txt` (numery linii) · `WYNIKI/testy-pc.txt` · `SHA256SUMS.txt` · `CZYTAJ-NAJPIERW.txt`.

# Firmware centrali 3.58.20 — Zew 2 (DHT22) poniżej 0 °C: klon koduje ujemne w U2

Data: 2026-11-27 · następna po **3.58.19** (`firmware-patch/v3.58.19-plynny-deszcz/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 459 linii, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.19: **6 hunków, 24 linie kodu** (reszta to komentarze i wpis w historii), wyłącznie C++
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

## Zmiana
`zew2DekodujU2(float*)` w bloku `ZEW2-FILTR` (testowalnym na PC): jeśli biblioteka zwróci `t` z przedziału
**−3276,75..−3236,75** (obraz U2 zakresu −0,1..−40,0 °C — prawdziwy DHT22 nigdy nie oddaje takich wartości),
to `t = −lround((t + 3276,8)·10)/10`. Wywołanie w `wilgotnosc()` zaraz po `read_dht()`, tylko przy `DHT_OK`,
**przed** `zew2RamkaPoprawna()` (zakres −40..80 sprawdzany na przeliczonej wartości) i przed filtrem skoków
(odkodowana wartość przechodzi tę samą weryfikację co każda inna). Pierwsze wykrycie: jedna linia WARN
`DHT22 Zew2 zdarzenie=u2 surowa=… t=… msg="klon koduje ujemne w U2, przeliczam"`; w linii INFO
`DHT22 diagnostyka` nowe pole `u2=<licznik>`.

Wartości dodatnie, prawdziwe ujemne ze znakiem w bicie 15, 0,0/−0,0, NaN, U2 poniżej −40 °C i śmieci
(−3276,8, +3276,7, RH > 100) — bez zmian w zachowaniu. Skutek uboczny na plus: **wilgotność** z DHT22 przy
mrozie przestaje zamierać (do 3.58.19 odpadała razem z temperaturą).

## Testy na PC (`tests/uruchom_testy.sh`, wynik w `WYNIKI/testy-pc.txt`)
- **A** blok `ZEW2-FILTR` wycięty z pliku: 13 scenariuszy z 3.58.10 (regresja) + `[14]` dekoder na granicach,
  `[15]` wszystkie 400 wartości −0,1..−40,0 °C w obu kodowaniach przez dokładną formułę biblioteki,
  `[16]` scenariusz z 26.11 — **98 sprawdzeń, 0 błędów** (także ASan+UBSan).
- **B** prawdziwa `wilgotnosc()` z atrapami: 9 scenariuszy z 3.58.10 + `[10]` klon U2 poniżej zera
  (Zew 2 −0,1/−0,3/−1,2, RH ACCEPTED, WARN raz, `u2=3`, `jsonTempDiag() = "-0.1"`), `[11]` śmieci nadal INVALID —
  **63 sprawdzenia, 0 błędów**.
- **C** znaczniki 3.58.20, kolejność `read_dht → zew2DekodujU2 → zew2RamkaPoprawna`, `zew2RamkaPoprawna()` bez zmian,
  plik = 3.58.19 + patch (SHA), HTML i kod poza blokiem/`wilgotnosc()` identyczny z 3.58.19.
- **D** pełne testy paczki 3.58.19 (i przez nią 3.58.18…3.58.14) na tym pliku.

## Pliki
`src/main_centrala.cpp` · `main_centrala-v3.58.19-do-v3.58.20.patch` · `tests/` (`uruchom_testy.sh`,
`wytnij_zew2_filtr.py`, `wytnij_wilgotnosc.py`, `test_zew2_filtr.cpp`, `test_wilgotnosc.cpp`) ·
`WYNIKI/zakres-zmian.txt` (numery linii) · `WYNIKI/testy-pc.txt` · `SHA256SUMS.txt` · `CZYTAJ-NAJPIERW.txt`.

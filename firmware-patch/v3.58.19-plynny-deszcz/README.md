# Firmware centrali 3.58.19 — płynne odliczanie „deszcz za N min” / „pada N min”

Data: 2026-11-23 · następna po **3.58.18** (`firmware-patch/v3.58.18-pierwsza-prognoza/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 401 linii, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.18: **11 hunków, ok. 59 linii kodu** (reszta to komentarze i wpis w historii), wyłącznie C++.
HTML/JS strony WWW bajt w bajt identyczny z 3.58.18 (sprawdzane testem). **Panel LCD bez zmian** (v1.5.17).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Co i dlaczego
Pytanie z 23.11: czy „deszcz za N min” na kafelku ZEWN odlicza co minutę, czy skacze? Do 3.58.18 — skakało:
`deszcz_min` było liczbą slotów 15-min × 15 z ostatniego pobrania; stało przez kwadrans, potem spadało o 15
(albo o 30, gdy pobranie trafiło tuż przed granicą slotu). Przyczyny dwie: (1) `current.time` Open-Meteo to
**początek** bieżącego slotu, nie „teraz”, więc minuty już zużyte w slocie nie były odejmowane; (2) między
pobraniami nic nie liczyło upływu czasu.

Teraz (`deszczStanTeraz()`, nowe globalne `deszczStartMillis` / `deszczKoniecMillis` / `deszczKoniecZnany` / `deszczKoniecMinuty`):

| moment | co robi centrala |
|---|---|
| pobranie (co 15 min, pierwsze po 60 s) | liczy `przesMin` = ile minut bieżącego slotu już minęło wg własnego zegara (tylko gdy RTC/NTP wiarygodne i 0 ≤ r < 15, inaczej 0 = jak dotąd), odejmuje je od minut do początku/końca opadu, zapamiętuje **chwile** `millis()` początku i końca (dla stanu 1 szuka też końca zapowiadanego opadu, ta sama histereza 2 suchych slotów) |
| każde `/api/status` | stan 1: `deszcz_min = ceil((start − millis())/60 s)`; gdy ≤ 0 → stan 2 „pada” z czasem do końca; stan 2 bez znanego końca → `-1` („pada” bez czasu); po końcu → stan 0 — wszystko do następnego pobrania, które koryguje wg nowej prognozy |

Dodatkowo: koniec opadu ≤ 0 min przy pobraniu (sucho w bieżącym slocie, choć `current.precipitation` mówi, że pada)
→ `-1` („pada” bez czasu) zamiast „pada 0 min”. Różnice `millis()` w `unsigned long` — poprawne przy przepełnieniu po 49,7 dnia
(test 32-bit). Log `DESZCZ zdarzenie=stan` ma teraz `w_slocie_min=` i `koniec_za_min=`.
Na LCD: „deszcz za 113 min” → po minucie „za 112” → … → „za 1” → „pada 105 min” → … → „pada 1 min” → ikona bez deszczu.

## Pliki
| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.19 |
| `main_centrala-v3.58.18-do-v3.58.19.patch` | `patch -p1` z katalogu projektu na plik 3.58.18 |
| `tests/uruchom_testy.sh` | A: parsowanie + odliczanie (prawdziwy kod skompilowany g++ 64-bit i 32-bit jak ESP32, 13 przypadków: 6 dotychczasowych + 7 nowych z zegarem i upływem `millis()`), `/api/status` z `deszczStanTeraz()`, wersja, HTML = 3.58.18, `node --check`; B: SHA po patchu; C: testy 3.58.18 → 3.58.17 → 3.58.16 → 3.58.15 → 3.58.14 |
| `tests/test_pogoda_parsowanie.py`, `tests/harness_pogoda.cpp.in` | harness (stuby `millis()`, zegara RTC, `String`, `DLOG`) |
| `SHA256SUMS.txt`, `CZYTAJ-NAJPIERW.txt`, `WYNIKI/` | jak w poprzednich paczkach |

# Firmware centrali 3.58.18 — pierwsza prognoza ~1 min po starcie (było: 15 min)

Data: 2026-11-23 · następna po **3.58.17** (`firmware-patch/v3.58.17-pogoda-temp/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 302 linii, SHA-256 `27555cb0…`).
Patch względem 3.58.17: **5 hunków, 6 linii kodu** (reszta to komentarze i wpis w historii), wyłącznie C++.
HTML/JS strony WWW jest bajt w bajt identyczny z 3.58.17 (sprawdzane testem).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Co i dlaczego
Po wgraniu 3.58.17 + panelu v1.5.17 na płytkę bez czujników kafelek ZEWN pokazywał „--” zamiast „~3°C”;
`/api/status` miało `pogoda_kod:-1`, `pogoda_temp:null` przy ~6 min od startu. Przyczyna nie leżała w nowych
polach: obie bramki Open-Meteo w zadaniu TG startowały z `owXxxLastFetchMillis = 0` i warunkiem
`millis() - last >= 15 min`, więc **pierwsze pobranie szło dopiero 15 minut po włączeniu** — tak było od
v2.13.0 (po każdym restarcie kwadrans bez „deszcz za 6 h”, ikony pogody i prognozy na WWW/LCD).

Teraz liczniki startują „cofnięte” o interwał:

| zapytanie | pierwsze pobranie | kolejne |
|---|---|---|
| DESZCZ (`pobierzPrognozeDeszczu`, pola `deszcz_*`, `pogoda_*`) | ~60 s po starcie | co 15 min (bez zmian) |
| PROGNOZA (`pobierzPelnaPrognoze`, ekran prognozy WWW/LCD) | ~120 s po starcie | co 15 min (bez zmian) |

Rozsunięte o minutę, żeby dwa zapytania HTTPS nie trafiły w tę samą iterację zadania TG. Warunki „Wi-Fi
połączone” i budżet iteracji (`TG_ITER_HAS_BUDGET`) bez zmian — jeśli Wi-Fi wstanie później niż po minucie,
pobranie idzie zaraz po połączeniu. Liczba zapytań na dobę bez zmian (+1 na restart). Arytmetyka `unsigned`
(`0UL - X`) jest celowa: `millis() - (0 - X) == millis() + X`, ten sam mechanizm, na którym opiera się każda
bramka `millis()` przy przepełnieniu — test sprawdza to przy 32-bitowym `unsigned long` jak na ESP32.

## Pliki
| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.18 |
| `main_centrala-v3.58.17-do-v3.58.18.patch` | `patch -p1` z katalogu projektu na plik 3.58.17 |
| `tests/uruchom_testy.sh` | A: bramka startu (prawdziwe definicje + prawdziwy warunek z zadania TG skompilowane g++ 32/64-bit: 60 s / 120 s / co 15 min / przepełnienie `millis()`), wersja, „zmienione tylko linie liczników”, HTML = 3.58.17, `node --check`; B: SHA po patchu; C: testy 3.58.17 → 3.58.16 → 3.58.15 → 3.58.14 |
| `tests/test_bramka_startu.py` | harness bramki |
| `SHA256SUMS.txt`, `CZYTAJ-NAJPIERW.txt`, `WYNIKI/` | jak w poprzednich paczkach |

# Firmware centrali 3.58.16 — kod pogody (WMO) i porywy wiatru w `/api/status` dla kafelka ZEWN na LCD

Data: 2026-11-23 · następna po **3.58.15** (`firmware-patch/v3.58.15-bez-historia-csv/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 223 linii, SHA-256 `f8d0c997…`).
Patch względem 3.58.15: **12 hunków, +105 linii**, wyłącznie C++ (prognoza deszczu i `/api/status`).
HTML/JS strony WWW jest bajt w bajt identyczny z 3.58.15 (sprawdzane testem).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Co i dlaczego
Użytkownik wybrał w projektancie kafelka ZEWN (`LCD-Centrali/docs/symulator_kafelka_zewn.html`, 23.11)
wariant „ikony=kolor, napis=zawsze, układ=F”: kafelek ma pokazywać **każde zjawisko** (śnieg, mżawkę,
burzę, grad, mgłę, silny wiatr), a bez opadu stan nieba — nie tylko „deszcz za 6 h”. Centrala dokłada
do **istniejącego** zapytania Open-Meteo (co 15 min, `pobierzPrognozeDeszczu()`) pola `weather_code`,
`is_day`, `wind_gusts_10m` (teraz) i tablicę `minutely_15.weather_code`, a w `/api/status` wystawia:

| pole | znaczenie | brak |
|---|---|---|
| `pogoda_kod` | kod WMO pogody teraz | -1 |
| `pogoda_kod_opad` | kod WMO w slocie, do którego odnosi się `deszcz_stan`/`deszcz_min` (gdy pada = kod teraz) | -1 |
| `pogoda_dzien` | 1 dzień / 0 noc | -1 |
| `wiatr_porywy` | porywy wiatru teraz, km/h (1 miejsce po przecinku) | -1 |

Centrala podaje surowe liczby; napis, ikona i próg „silny wiatr” (55 km/h) są po stronie panelu LCD **v1.5.14**.
`deszcz_stan`/`deszcz_min` bez zmian. Kody WMO: 0 bezchmurnie, 1–2 częściowo, 3 pochmurno, 45/48 mgła,
51–57 mżawka, 61–67/80–82 deszcz, 71–77/85–86 śnieg, 95 burza, 96/99 burza z gradem.

## Pliki
| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.16 |
| `main_centrala-v3.58.15-do-v3.58.16.patch` | `patch -p1` z katalogu projektu na plik 3.58.15 |
| `tests/uruchom_testy.sh` | A: parsowanie Open-Meteo (prawdziwy kod z pliku skompilowany g++, 5 odpowiedzi), pola statusu, URL, HTML = 3.58.15, `node --check`; B: SHA po patchu; C: testy 3.58.15 → 3.58.14 → 3.58.13/3.58.11 |
| `tests/test_pogoda_parsowanie.py`, `tests/harness_pogoda.cpp.in` | harness: atrapa `String` + wycięte z pliku funkcje pomocnicze i ciało parsowania |
| `SHA256SUMS.txt`, `CZYTAJ-NAJPIERW.txt`, `WYNIKI/` | jak w poprzednich paczkach |

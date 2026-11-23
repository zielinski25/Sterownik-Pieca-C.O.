# Firmware centrali 3.58.17 — temperatura z prognozy (`pogoda_temp`) w `/api/status` dla kafelka ZEWN na LCD

Data: 2026-11-23 · następna po **3.58.16** (`firmware-patch/v3.58.16-pogoda-zewn/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 266 linii, SHA-256 `93dab586…`).
Patch względem 3.58.16: **9 hunków, +48/−5 linii**, wyłącznie C++ (prognoza deszczu i `/api/status`).
HTML/JS strony WWW jest bajt w bajt identyczny z 3.58.16 (sprawdzane testem).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Co i dlaczego
Użytkownik wybrał w projektancie kafelka ZEWN (`LCD-Centrali/docs/symulator_kafelka_zewn.html`, 23.11 wieczór)
wariant „układ=E, brak_czujnika=prognoza”: gdy czujnik zewnętrzny DS18B20 nie działa (centrala podaje
`-1.0`), kafelek ma pokazać przybliżoną temperaturę z prognozy („~3°C”) zamiast „--”. Centrala dokłada
do **istniejącego** zapytania Open-Meteo (co 15 min, `pobierzPrognozeDeszczu()`) pole `current.temperature_2m`
i w `/api/status` wystawia, zaraz za `wiatr_porywy`:

| pole | znaczenie | brak |
|---|---|---|
| `pogoda_temp` | temperatura powietrza 2 m z Open-Meteo, °C, jedno miejsce po przecinku | `null` |

Brak to `null`, nie `-1` jak w polach z 3.58.16 — temperatura `-1 °C` jest prawidłową wartością.
Wewnątrz centrali znacznik braku to `POGODA_TEMP_BRAK = -99`; odczyty spoza −70…60 °C są odrzucane.
Pozostałe pola (`pogoda_kod`, `pogoda_kod_opad`, `pogoda_dzien`, `wiatr_porywy`, `deszcz_stan`, `deszcz_min`) bez zmian.
Co z tą liczbą zrobić (tylda, zaokrąglenie, kiedy pokazać) decyduje panel LCD **v1.5.17** — centrala podaje surową wartość.

## Pliki
| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.17 |
| `main_centrala-v3.58.16-do-v3.58.17.patch` | `patch -p1` z katalogu projektu na plik 3.58.16 |
| `tests/uruchom_testy.sh` | A: parsowanie Open-Meteo (prawdziwy kod z pliku skompilowany g++, 6 odpowiedzi: −3,5 / 7,2 / 0,0 / bez pola / null / jak dotąd), pole statusu, URL, wersja, HTML = 3.58.16, `node --check`; B: SHA po patchu; C: testy 3.58.16 → 3.58.15 → 3.58.14 → … |
| `tests/test_pogoda_parsowanie.py`, `tests/harness_pogoda.cpp.in` | harness: atrapa `String` + wycięte z pliku funkcje pomocnicze i ciało parsowania |
| `SHA256SUMS.txt`, `CZYTAJ-NAJPIERW.txt`, `WYNIKI/` | jak w poprzednich paczkach |

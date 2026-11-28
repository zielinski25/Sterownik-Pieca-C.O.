# Firmware centrali 3.58.15 — usunięty przycisk „HISTORIA CSV” ze strony WWW

Data: 2026-11-26 · następna po **3.58.14** (`firmware-patch/v3.58.14-zrodlo-zew/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 118 linii, SHA-256 `8bfab4e5…`).
Patch względem 3.58.14: **4 hunki**, wyłącznie HTML strony WWW wbudowanej w plik + nagłówek,
historia, numer wersji. C++ za HTML-em jest bajt w bajt identyczne z 3.58.14 (sprawdzane testem).

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Co i dlaczego
Zrzut ekranu z 23.11: stały przycisk „⬇ HISTORIA CSV” w lewym dolnym rogu z okienkiem
„Pobierz historię: Krótka (24 h) / Długa (archiwum)” — „to nie jest potrzebne”. Usunięty HTML
(`histBtn`, `histPanel`, `histShort`, `histLong`, `histInfo`) i jego `<script>` z [v3.56.1 HISTORIA-POBRANIE].
Trasy `/history/link`, `/history`, `/history/long`, `/api/history*` zostają. `Piec.html` nie miał tego przycisku.

## Pliki
| plik | co |
|---|---|
| `src/main_centrala.cpp` | pełny plik 3.58.15 |
| `main_centrala-v3.58.14-do-v3.58.15.patch` | `patch -p1` z katalogu projektu na plik 3.58.14 |
| `SHA256SUMS.txt`, `CZYTAJ-NAJPIERW.txt`, `WYNIKI/`, `tests/uruchom_testy.sh` | jak w poprzednich paczkach |

# data/ — historia wgrywana do LittleFS (`pio run -t uploadfs`)

Ten katalog trafia **1:1** do partycji LittleFS w płytce. Firmware czyta z niej:

| plik | zawartość | format |
|---|---|---|
| `/hist_short.csv` | historia krótka (godziny) | **14 kolumn** — dokładnie jak `GET /api/history` |
| `/hist_long.csv` | historia długa (dni) | **35 kolumn** — dokładnie jak `GET /api/history/long` |

Są to **te same nazwy i ten sam format**, których używa prawdziwa centrala
(`main_centrala.cpp`: `HISTORIA_PLIK "/hist_short.csv"`, `HISTORIA_DLUGI_PLIK`),
więc pliki skopiowane z centrali wchodzą bez żadnej konwersji.

## Jak wgrać historię z prawdziwej centrali

1. Skopiuj z centrali (np. przez `mpremote`, eksport z aplikacji albo z karty)
   pliki `hist_short.csv` i `hist_long.csv` **do tego katalogu**.
2. Z katalogu projektu:

   ```powershell
   pio run -e esp32 -t uploadfs      # zapisuje data/ -> LittleFS
   ```

   (samo `pio run -t upload` wgrywa firmware; `uploadfs` — dane; można w dowolnej
   kolejności, ale po `uploadfs` zrestartuj płytkę albo wpisz w konsoli `import`.)

3. Konsola po starcie pokaże, ile wierszy udało się wczytać:

   ```
   [HIST] /hist_short.csv: wczytano 1440 wierszy, pominietych 0
   [HIST] /hist_long.csv: wczytano 120 wierszy, pominietych 0
   [HIST] z plikow: krotka=360, dluga=120 (panel rysuje PRAWDZIWA historie)
   ```

   Bufor krótki ma 360 próbek (6 h), więc z pliku wchodzi **ostatnie 360
   wierszy**; długi ma 120 próbek.

4. Gdy katalog jest pusty (tak jak teraz), firmware **sam zasiewa** 360 + 24
   wymyślone próbki — wykresy działają od razu, bez plików.

## Czego firmware NIE przyjmie

* wiersza z inną liczbą kolumn (14 / 35) — zostaje pominięty i wypisany w konsoli
  (`[HIST] pominiety wiersz (...)`), reszta pliku wchodzi normalnie,
* nagłówka CSV — jeśli Twój plik ma linię z nazwami kolumn, po prostu ją usuń
  (albo zostaw: zostanie pominięta jako wiersz złą liczbą kolumn),
* innej kolejności kolumn niż w `/api/history` — kolejność jest częścią kontraktu.

CRLF (pliki z Windows) jest obsługiwany.

# Historia wykresów na nową (gołą) płytkę ESP32-S3

`scal_historie.py` scala dowolną liczbę pobranych kopii `hist_short*.csv` / `hist_long*.csv`
(Drive, Telegram `/historia`, `/history?k=`) w dwa pliki `data/hist_short.csv` (1440 ostatnich
wierszy) i `data/hist_long.csv` (8000 ostatnich wierszy) w formacie i limitach firmware centrali
(`historiaWczytajZFS()` / `historiaDlugaWczytajZFS()`), do wgrania przez
`pio run -e esp32-s3-n16r8 -t uploadfs` (wymaga `board_build.filesystem = littlefs`).

`--na-teraz` (tylko do podglądu) przesuwa całą historię tak, by kończyła się teraz — okna
24 h / 7 dni / 30 dni na WWW i LCD są od razu pełne, ale daty są nieprawdziwe.

Instrukcja krok po kroku: `CZYTAJ-NAJPIERW.txt`. Test: `python3 tests/test_scal_historie.py`
(21 sprawdzeń). Zmian w firmware nie ma.

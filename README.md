# Sterownik-Pieca-C.O.

Sterownik pieca C.O. (ESP) — repozytorium przebudowane od zera.

## Struktura

- `firmware/` — firmware ESP (main_centrala.cpp)
- `panel/` — panel lokalny (Piec.html)
- `docs/` — dokumentacja i konfiguracja

## Narzędzia (`narzedzia/`)

- `nvs_podglad.html` — podglądarka graficzna zrzutu NVS (jeden plik, bez
  instalowania: otwórz w przeglądarce i przeciągnij `nvs.bin`); przykład
  w środku, opisy kluczy tego projektu
- `nvs_odczyt.py` — odczyt całej partycji NVS ze zrzutu `esptool` (wszystkie
  przestrzenie i klucze, także usunięte; sam znajduje partycję NVS w zrzucie,
  czyta też tablicę partycji); instrukcja: `NVS_ODCZYT_JAK.md`
- `ota_panelu_sprawdz.py` — sprawdzenie przed aktualizacją panelu LCD
- `pamiec_centrali.py` — rachunek pamięci centrali z kodu
- `analiza_pieca.py`, `analiza_zaawansowana.py` — analiza eksportu telemetrii

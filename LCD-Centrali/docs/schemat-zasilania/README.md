# Schemat zasilania panelu LCD z ogniwa Li-ion (TP4056 → MT3608 → ESP32)

Dwie strony HTML, bez zależności – otwórz w przeglądarce (dwuklik) albo:

| Plik | Co zawiera |
|---|---|
| `symulacja.html` | Schemat ideowy + **symulacja**: USB/ogniwo/napięcie/MT3608/czas bez dotyku → stany GPIO34/35/14/13, linia logu `tag=BATERIA`, decyzja kroku 3 (usypianie), bilans prądów i czas pracy. Obliczenia przepisane z `src/main.cpp` 1.4.7 (`zasilanieUSB()`, `bateriaOdczytajNapiecie()`, tabela `TABELA_BAT`). |
| `index.html` | **Widok montażowy** (strona startowa): rysunki modułów TP4056 (03962A), MT3608, DevKit V1 30-pin, 18650, elementy dyskretne z kolorami pasków, 19 przewodów z podświetlaniem i opisem; lista zakupów. |

Podgląd online (bez pobierania):
- https://htmlpreview.github.io/?https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./main/LCD-Centrali/docs/schemat-zasilania/symulacja.html
- https://htmlpreview.github.io/?https://raw.githubusercontent.com/zielinski25/Sterownik-Pieca-C.O./main/LCD-Centrali/docs/schemat-zasilania/index.html

Założenia sprzętowe (ustalone 2026-11): ogniwo → TP4056 z DW01A → MT3608 nastawiony na 5,0–5,1 V → `VIN` DevKita;
dzielnik 100k/100k + 1 µF z `OUT+` na GPIO34; CHRG+STDBY (pady po wylutowanych LED) + 10k do 3V3 na GPIO35;
przycisk GPIO14→GND; piezo pasywny GPIO13→GND. Wspólna masa z `OUT−`, nigdy z `B−`.

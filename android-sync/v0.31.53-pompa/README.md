# 0.31.53 — pompa na całą wysokość wykresu

Paczka nakładkowa: **3 pliki**. Data 2026-10-29.
**Zawiera model z 0.31.52** — jeśli go nie wgrałeś, ta paczka wystarcza.

## Co naprawia

Zgłoszenie: *„wcześniej była do samej góry zaznaczona, a teraz nisko takie
pierdki"*. Od 0.31.34 praca pompy w trybie mieszanym była paskiem **10 dp**
przy dolnej krawędzi. Przy zakresie 30 dni impulsy 2–3 px przestawały być
czytelne.

**Teraz:** praca pompy to **półprzezroczysta kolumna na całą wysokość**
(wypełnienie 18%, krawędzie 55%), rysowana **pod** temperaturami. Widać ją
jak przed 0.31.34, ale nie zasłania linii temperatur.

Tryb „tylko POMPA" bez zmian: zwykła linia schodkowa, oś PRACA / STOP.

## Dowód

`narzedzia/pompa-wykres-2026-10-29/pompa-przed-po.png` — ten sam fragment
historii (9–11.10, 3905 próbek, 393 z pompą ON) narysowany przed i po.

## Co jest w paczce

| plik | zmiana |
|---|---|
| `core/SolarThermalModel.kt` | model z niebem z 0.31.52 (bez zmian) |
| `ui/chart/ChartEngine.kt` | kolumny pracy pompy na pełną wysokość |
| `app/build.gradle.kts` | 31053 / „0.31.53" |

## Czego NIE rusza

- **Nazwy czujników zostają** — „Panel słoneczny", „Kolektor". Wersja 0.31.53
  z nazwami „zbiornik" została wycofana; **to nie jest ta paczka**.
- Firmware bez zmian — 3.56.7 zostaje, 3.56.8 czeka.
- Reszta aplikacji bez zmian.

## Sprawdzenie paczki

- ZIP: **21 965 B**
- SHA-256: `5028f279791f953bad760f07175ab7cc6d2e73c2218eddd03694b6789b76b07d`
- 3 pliki, `unzip -t` bez błędów, 3/3 ma **identyczny** SHA-256 jak w repo
- `ChartEngine.kt`: kompilator Kotlina — **zero błędów składni**

## Jak sprawdzić po wgraniu

1. Ustawienia → „v0.31.53".
2. Wykresy → 30 dni → włącz TEMPERATURY + POMPA.
3. Praca pompy = zielona kolumna **do górnej krawędzi**, nie kreska przy osi.
4. Linie temperatur widoczne **nad** kolumną.

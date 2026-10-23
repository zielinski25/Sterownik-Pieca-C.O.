# Android v0.31.28 — panel solarny bez sum narastających

Nakładka na drzewo v0.31.27. **3 pliki.** `versionCode = 3034`, `versionName = "v0.31.28"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.28-CZYTELNE.zip` |
| rozmiar | 21 222 B (4 wpisy) |
| sha256 | `0263c4fb28fa46870410ec180d5bb25e6bb73d2e44476b9ccccca26fb0e4fe6e` |

## Co usunięte

`Zysk narastająco` i `Pobory wody narastająco`. Algorytm liczył każdy wzrost,
a każdy spadek pomijał — drobne wahania czujnika sumowały się jak grzanie.
206 próbek → +93,1 °C przy wodzie 33 °C.

## Co zostało i jak się nazywa

```
Panel teraz / powietrze        33.2 °C / 18.0 °C
Różnica panel − powietrze             +15.2 °C
Zysk dziś · zmierzone                  +0.5 °C
Prognoza na dziś · szacunek            +1.5 °C
Pobrania ciepłej wody                   brak
```

Plus jedno zdanie pod tabelą: co znaczy „zmierzone", a co „szacunek".

Karta **DZIŚ** w siatce 7 dni pokazuje teraz prognozę, jak pozostałe dni —
pomiar jest w wierszu „zmierzone". Pod nagłówkiem sekcji dopisane:
`o ile °C może ogrzać się woda w zbiorniku 100 l · szacunek, nie pomiar`.

## Czego nie sprawdzono

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 0/0/0, usunięty nieużywany parametr `m` z `SolarDayCard`
(sygnatura + 2 wywołania), `diff -rq` = dokładnie 3 pliki.

## Build potwierdzony (2026-10-22)

```
BUILD SUCCESSFUL in 10s
37 zadań (12 wykonanych, 25 z cache)
versionCode = 3034, versionName = "v0.31.28"
```

Dwa ostrzeżenia, **oba sprzed tej zmiany**:

```
Weather.kt:649  'quadraticBezierTo' is deprecated. Use quadraticTo()
Weather.kt:655  'quadraticBezierTo' is deprecated. Use quadraticTo()
```

Rysowanie łuku słońca (Canvas), ~200 linii niżej niż zmieniany panel.

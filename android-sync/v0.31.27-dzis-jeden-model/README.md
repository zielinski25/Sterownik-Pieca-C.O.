# Android v0.31.27 — „DZIŚ" z próbek, jeden model prognozy

Nakładka na drzewo v0.31.26. **4 pliki.** `versionCode = 3033`, `versionName = "v0.31.27"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.27-DZIS.zip` |
| rozmiar | 25 558 B (5 wpisów) |
| sha256 | `a8edd4e5729c85a1fa7f8a8184a162058a706047610745389a107d3e86500ee6` |

## Defekty naprawione

1. `estGainC = maxOf(accumulatedGrossGain, modelGain)` → karta „DZIŚ" pokazywała
   sumę z całego bufora (+93,1 °C) zamiast dnia (+9,1 °C).
2. Prognoza z zachmurzenia (`(100−cloud)×8,5`) dawała +0,0 °C obok +19,0 °C
   z modelu dziennego — dwa modele w jednym ekranie.
3. Brak sufitu: model mógł pokazać dowolnie dużo dla zbiornika 100 l.

## Model

```
ΔT = min(GHI [kWh/m²] × 4,2 ; 45 °C)

4,2 = apertura 1,0 m² × sprawność 0,49 ÷ 0,1163 kWh/K
```

Apertura 10 rur 58/1800 wg katalogów: 0,90–1,12 m². Uzysk roczny 450–600 kWh/m².

## Czego nie sprawdzono

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się". Sprawdzone: bilans
nawiasów 0/0/0, importy `SolarAnalytics` i `roundToInt` w `Weather.kt`,
`grep forecastKwh` = 0, `diff -rq` = dokładnie 4 pliki.

## Poprawka po BUILD FAILED (wciąż 3033)

Pierwsza paczka nie kompilowała się:

```
e: SolarAnalytics.kt:188:49 None of the following candidates is applicable
```

`if (h.isDay) 1 else 0.0` — gałęzie `Int` i `Double`. Typ wspólny to
`Number`, a `Double * Number` nie istnieje. Naprawione na `1.0`.

**Pobierz ZIP jeszcze raz** — ten sam numer wersji, nowy plik.

## Build potwierdzony (2026-10-22, druga paczka)

```
BUILD SUCCESSFUL in 22s
37 zadań (6 wykonanych, 31 z cache)
versionCode = 3033, versionName = "v0.31.27"
```

Zero błędów. Ostrzeżenia — **wszystkie sprzed tej zmiany**:

```
AppModel.kt:234      VIBRATOR_SERVICE is deprecated
ChartEngine.kt:586-8 Condition is always 'true'   (x3)
Weather.kt:645,651   quadraticBezierTo is deprecated
SvgPath.kt:141,148   quadraticBezierTo is deprecated
Theme.kt:124,125     statusBarColor / navigationBarColor is deprecated
```

Żadne nie dotyczy `SolarAnalytics`, karty DZIŚ ani modelu prognozy.
Pierwsze podejście padło na `if (h.isDay) 1 else 0.0` — patrz §„Poprawka” wyżej.

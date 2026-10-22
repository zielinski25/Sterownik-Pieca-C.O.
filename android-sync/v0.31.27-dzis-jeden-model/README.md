# Android v0.31.27 — „DZIŚ" z próbek, jeden model prognozy

Nakładka na drzewo v0.31.26. **4 pliki.** `versionCode = 3033`, `versionName = "v0.31.27"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.27-DZIS.zip` |
| rozmiar | 25 556 B (5 wpisów) |
| sha256 | `41bc30e2f16b2d2f8e002cc56f82a85c9fa8d7d93fc220120c080ab69c31cd53` |

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

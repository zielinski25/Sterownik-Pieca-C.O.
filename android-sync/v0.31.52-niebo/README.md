# 0.31.52 — model z niebem

Paczka nakładkowa: **2 pliki**. Data 2026-10-29.

## Co jest w środku

| plik | zmiana |
|---|---|
| `android/app/build.gradle.kts` | `versionCode` 31052, `versionName` „0.31.52" |
| `android/app/src/main/java/com/sterownikco/pro/core/SolarThermalModel.kt` | człon „niebo", wyłączona cyrkulacja wsteczna, `Hour.cloudPct` |

## Co dokładnie zmieniono w modelu

```
C·dT/dt = Q − (tankUa + skyRadWk·(1−zachmurzenie)·noc)·(T−Ta)

tankUa        = 1,40 W/K   bez zmian (model dzienny nietknięty)
skyRadWk      = 8,20 W/K   NOWE — zbiornik na dachu patrzy w niebo
noc           = 1 przy GTI 0 → 0 przy GTI 50 W/m² (łagodne przejście)
reverseFlowKgS = 0,0       BYŁO 0,004 — rura heat-pipe to dioda termiczna
```

Zachmurzenie bierze się z prognozy godzinowej, którą aplikacja **już pobierała**
(`cloud_cover`) — żadnego nowego czujnika, żadnego nowego zapytania.

Gdy zachmurzenia nie ma w danych (`NaN`), model liczy **dokładnie tak jak
przed 0.31.52** — sprawdzone testem.

## Wynik — test na prawdziwym kodzie

Uruchomiony **ten sam** `SolarThermalModel.kt`, który jest w tej paczce.
11 prawdziwych nocy 18–28.10.2026, temperatura powietrza godzinowa z logu
centrali, zachmurzenie z Open-Meteo dla Trzemchy Górnej.

| | średni błąd temperatury o 06:00 |
|---|---|
| 0.31.51 | **2,07 °C** |
| 0.31.52 | **0,88 °C** |
| poprawa | **58%** |

Największa pojedyncza poprawa — noc 23/24.10: było 36,6 °C przy zmierzonych
28,5 °C (błąd 8,1), jest 29,1 °C (błąd 0,6).

Pliki testu: `narzedzia/symulator-solar-2026-10-29/test-0.31.52/`
(`Test.kt`, `TestDzien2.kt`, `WYNIK-NOCE-0.31.52.txt`, `WYNIK-DZIEN-0.31.52.txt`).

## Czego nie zmieniono

- **Firmware centrali** — bez zmian. 3.56.7 zostaje wgrany, 3.56.8 nadal czeka.
- **Reszta aplikacji** — bez zmian.
- Model dzienny przy pełnym zachmurzeniu: różnica ±0,1 °C.

## Sprawdzenie paczki

- ZIP: 9 780 B
- SHA-256: `209dea955090a6dbc1cb7fd227b563c7fd17b632ad1f7a03ac28cf4c1f711bd5`
- `unzip -t`: bez błędów
- 2/2 plików w ZIP mają **identyczny SHA-256** jak pliki w repozytorium
- `SolarThermalModel.kt` kompiluje się (`K2JVMCompiler`, 0 błędów)

## Uwaga o zachowaniu na ekranie

Wieczorem karta **„do końca dnia"** pokaże teraz **większy spadek** niż
w 0.31.51 — zwłaszcza przy czystym niebie. To nie regres, to poprawka:
0.31.51 grubo zaniżał nocne stygnięcie (średnio o 2,07 °C na noc).

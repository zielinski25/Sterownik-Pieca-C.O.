# Android v0.31.26 — wariant B: 100 l zamiast 200 kg, bez „~X kWh”

Nakładka na drzewo v0.31.25. **4 pliki do nadpisania.** `versionCode = 3032`,
`versionName = "v0.31.26"`.

| | |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.26-KWH.zip` |
| rozmiar | 23 380 B (5 wpisów: 4 pliki + README-WGRANIE.txt) |
| sha256 | `7f84017541bacaaac99ebeeacc45ab0c0f78f5424979c0bf4121691bd6c271e6` |

## Skąd decyzja

`ARCHIWUM_SOLARNE_STAN.md` **§14** (dawniej §12 — przenumerowane, bo §12
było już zajęte przez „Co dalej”). Ustalenia z §3: zbiornik **100 l**.

## Zmiany

| plik | zmiana |
|---|---|
| `core/SolarAnalytics.kt` | `estKwh()`: `* 200` → `* 100` → **0,1163 kWh/K** zamiast 0,2326 |
| `ui/screens/Weather.kt` | 3 miejsca bez kWh (2 kafelki + karta dnia) |
| `ui/screens/sheets/Sensors.kt` | 2 miejsca bez kWh |
| `app/build.gradle.kts` | 3032 / `v0.31.26` |

**Zostaje:** °C, ΔT, pokrycie, pobory wody, suma GHI, eksport CSV.
**Znika:** każda wyświetlana liczba kWh.

## Dlaczego kWh znikają, a nie tylko się zmniejszają

Trzy niepewności naraz (§10, §13):

1. GHI w rekordzie to **prognoza**, nie pomiar,
2. apertura (~1,4 m²) i sprawność nigdy nie były mierzone,
3. czujnik w połowie wysokości **nie widzi stratyfikacji** — energia raczej zawyżona.

Liczba wyglądająca na pomiar, którym nie jest, jest gorsza niż jej brak.

## Czego nie sprawdzono

Brak JDK/Kotlinc/Gradle → brak dowodu „kompiluje się”. Sprawdzone: bilans
nawiasów 3 plików Kotlina (0/0/0), `diff -rq` = dokładnie 4 pliki,
`grep` wyświetlanych kWh = 0 trafień (zostały 3 komentarze).

## Build potwierdzony (2026-10-22)

```
BUILD SUCCESSFUL in 12s
37 zadań (12 wykonanych, 25 z cache)
versionCode = 3032, versionName = "v0.31.26"
```

Dwa ostrzeżenia, **oba sprzed tej zmiany**:

```
Weather.kt:639  'quadraticBezierTo' is deprecated. Use quadraticTo()
Weather.kt:645  'quadraticBezierTo' is deprecated. Use quadraticTo()
```

To rysowanie łuku słońca (Canvas), ~300 linii niżej niż zmieniane kafelki.
Nie blokuje builda, nie dotyczy kWh ani przelicznika.

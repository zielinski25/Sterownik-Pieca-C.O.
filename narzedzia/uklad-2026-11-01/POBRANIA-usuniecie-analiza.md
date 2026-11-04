# „POBRANIA DZIŚ" — czy liczby z 04.11 są prawdziwe? (analiza przed zmianą kodu)

Data: 2026-11-04, ~22:46. Źródło: zrzut użytkownika (`Screenshot_...22-46-46...jpg`)
+ kod 0.31.76 + zapytanie do Open-Meteo dla 51,066 / 21,509.

## 1. Co pokazał ekran

| kafelek | wartość |
|---|---|
| ⚡ SPADEK DZIŚ (pomiar) | **−15,8 °C** |
| 🌡️ PANEL − POWIETRZE | −5,3 °C |
| 🚰 POBRANIA DZIŚ | **5 razy · −23,3 °C** |
| 🔮 DO KOŃCA DNIA (prognoza) | −0,1 °C |
| DZIŚ (karta 7-dniowa) | −15,9 °C (= −15,8 + −0,1) |
| pogoda dla tych współrzędnych, 22:45 | powietrze 1,8 °C, odczuwalna −1,1 °C |

## 2. Dowód, że „pobrania" są przeszacowane — bez żadnych założeń

Fizyka: woda traci ciepło **tylko** przez pobrania i przez wychładzanie.
Zatem suma ochłodzeń od pobrań **nie może przekroczyć** całkowitego spadku doby.

```
spadek netto zmierzony dziś ......... 15,8 °C   (pierwsza próbka dnia − ostatnia)
suma „pobrań" wg aplikacji .......... 23,3 °C
```

**23,3 > 15,8 → co najmniej 7,5 °C jest policzone podwójnie.** A to jeszcze
dolna granica, bo woda w ciągu dnia też się nagrzewała (słońce).

Kontrola energetyczna: zbiornik 100 l, 1 °C = 0,116 kWh.
−23,3 °C = **2,7 kWh** odebranego ciepła. Dziś szczyt nasłonecznienia to
**273 W/m²** (karta DZIŚ), czyli kolektor dał rano najwyżej ~1 kWh.
Nie da się odebrać 2,7 kWh ciepła ze zbiornika, do którego wpłynęło ~1 kWh.

Wniosek: użytkownik ma rację — **to nie są pobrania**. To heurystyka łapiąca
każdy spadek ≥ 0,5 °C utrzymany 5 minut, a taka definicja nie odróżnia
pobrania wody od zwykłego wychładzania nocnego i od drgnięć czujnika.

## 3. Jak to jest policzone (kod 0.31.76)

`core/SolarAnalytics.kt`:

| element | linia | rola |
|---|---|---|
| `DRAW_STEP_C = 0.5`, `DRAW_WINDOW_MS = 5 min`, `DRAW_HOLD_MS = 5 min`, `DRAW_MERGE_MS = 20 min`, `DRAW_FAULT_C = 12 °C` | 453–461 | progi heurystyki |
| `recomputeDraws()` | 243–305 | szuka spadków i scala epizody bliższe niż 20 min |
| `recomputeToday()` | 125–145 | liczy `todayDrawCount` / `todayDrawDrop` z tych flag |
| eksport CSV | 541 | kolumny `isDraw`, `drop` |
| odczyt archiwum | 84 | `isDraw` wracający z cache |

UI pokazuje to w dwóch miejscach:
- `ui/screens/Weather.kt:354` — kafelek „🚰 POBRANIA DZIŚ" w BILANSIE,
- `ui/screens/sheets/Sensors.kt:378` — wiersz „Pobrania ciepłej wody",
- `Sensors.kt:390` — nota wyjaśniająca próg 0,5 °C / 5 min.

Czego **nie** dotyka ta zmiana: „SPADEK DZIŚ" (−15,8) i karta DZIŚ (−15,9)
liczą się z różnicy temperatur (`todayGain`), a nie z pobrań. Zostają bez zmian.
Do usunięcia zostaje 1 kafelek (grid 4 → 3 karty, czyli równy rząd), 1 wiersz
w arkuszu Czujniki i 1 zdanie noty.

## 4. Czego NIE da się zrobić: wykryć prawdziwe pobranie

W telemetrii nie ma sygnału zaworu (sprawdzone: brak `valve`/`zawor` w
`core/*.kt` i w kluczach RTDB). Bez niego każde wykrywanie pobrania to
zgadywanie z przebiegu temperatury — czyli dokładnie to, co dziś pokazało
5 fałszywych pobrań. Uczciwe opcje: usunąć albo nazwać „orientacyjne".

## 5. Warianty

| # | zakres | efekt | koszt |
|---|---|---|---|
| **A** | usunąć kafelek + wiersz w Czujnikach + notę + całą heurystykę (`recomputeDraws`, `DRAW_*`, kolumny CSV zapisywane jako 0) | ekran przestaje kłamać, kod uboższy o ~70 linii | tracimy dane w CSV na przyszłość (już nie będą liczone) |
| **B** | usunąć tylko z ekranu; heurystyka liczy się dalej, `isDraw`/`drop` zostają w CSV | ekran czysty, historia i CSV bez zmian | kod nadal nosi nieużywaną funkcję |
| **C** | zostawić, ale pokazywać tylko gdy spadek jest szybszy niż fizyczne wychładzanie (próg z modelu cieplnego) | może łapać prawdziwe pobrania | wymaga danych z kilku dni, dziś nie do zrobienia |

**Rekomendacja: B na teraz** (zero kłamstwa na ekranie, zero utraty historii,
CSV dalej zbiera materiał) → a gdy zbierzemy dane, wrócić do C albo usunąć
heurystykę całkiem (A). B i A różnią się wyłącznie tym, czy licznik dalej
chodzi w tle.

## 6. Co dokładnie zmienić w B (do akceptacji)

1. `Weather.kt` — usunąć `MCard("🚰 POBRANIA DZIŚ", ...)` → grid 4 → 3 karty.
2. `Sensors.kt` — usunąć `SolarMetricRow("Pobrania ciepłej wody", ...)`.
3. `Sensors.kt` — usunąć z noty zdanie „Pobranie liczy się jako spadek
   o co najmniej 0,5 °C utrzymany ponad 5 minut…".
4. `SolarAnalytics.kt` — **bez zmian** (heurystyka i CSV zostają).
5. Wersja: 0.31.77 / `versionCode 31077`; firmware **3.57.9 bez zmian**.
6. Bramka: dodać sprawdzenia „brak POBRANIA DZIŚ na ekranie" i „nota nie
   obiecuje wykrywania pobrań".

W wariancie A dochodzi: usunięcie `recomputeDraws()` + stałych `DRAW_*`
+ pól `todayDrawCount/todayDrawDrop/drawCount/waterDrawEvents`, a kolumny
CSV zostają (zapisują 0), żeby stare pliki i czytniki się nie rozjechały.

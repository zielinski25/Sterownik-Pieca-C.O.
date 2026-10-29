# Model cieplny kolektora słonecznego — Trzemcha Górna

**Data:** 2026-10-23
**Dotyczy:** instalacja solarnej podgrzewania CWU przy centrali pieca C.O.
**Status:** model sprawdzony na 4 dniach pomiarów; **nie jest wgrany do aplikacji**

---

## 1. Po co ten dokument

Aplikacja pokazywała „ZYSK DZIŚ +21,5 °C", gdy fizyczne maksimum wynosiło 15,6 °C.
Prognoza pokazywała +11,2 °C, gdy naprawdę było +15,2 °C.

Powody były dwa i oba są w tym dokumencie opisane:

1. **Zły algorytm pomiaru** — sumował każdy wzrost temperatury, pomijał każdy spadek.
2. **Złe wejście do prognozy** — używała nasłonecznienia na płaszczyźnie poziomej,
   a kolektor stoi pod kątem.

Ten dokument zapisuje model, który te dwa błędy usuwa, oraz dowód, że działa.

---

## 2. Sprostowanie: lokalizacja

Wcześniejsze obliczenia z 2026-10-23 wykonałem dla **Radomia**. To był błąd.

Kolektor stoi w **Trzemchy Górnej**, powiat lipski, gmina Sienno.

```
51.066389° N   21.509167° E   ~185 m n.p.m.
```

**Firmware od początku miał dobre współrzędne.** Nic tu nie trzeba poprawiać:

```cpp
// main_centrala.cpp:22870
#define SOLAR_ARCHIVE_GHI_LAT  "51.066389"
#define SOLAR_ARCHIVE_GHI_LON  "21.509167"
```

Sprawdzenie — prognoza GHI dla Trzemchy vs to, co zapisało archiwum centrali:

| dzień | Open-Meteo (Trzemcha) | archiwum centrali | różnica |
|---|---|---|---|
| 2026-10-20 | 0,50 kWh/m² | 0,55 | −0,05 |
| 2026-10-21 | 2,59 | 2,47 | +0,12 |
| 2026-10-22 | 0,38 | 0,19 | +0,19 |
| 2026-10-23 | 2,66 | 2,65 | +0,01 |

Centrala liczy dla właściwego miejsca. Błąd był wyłącznie w moich obliczeniach.

---

## 3. Instalacja — dane wejściowe

| element | wartość | uwagi |
|---|---|---|
| kolektor | próżniowy rurowy, **10 rur** 58/1800 | heat pipe |
| apertura | **0,95 m²** | 2,84 m² / 30 rur × 10 |
| zbiornik | **100 l** | jeden z dwóch bojlerów |
| obieg | **termosyfon, bez pompy** | brak przepływu wymuszonego |
| czujnik | w wodzie, z boku, **w połowie wysokości** | `czujnik_temp_Panel_Plus`, BUS_3 |
| kanał telemetrii | `TELEMETRY_CH_PANEL` | zakres −30…160 °C |
| zima | kolektor nieużywany, woda spuszczona | model nie ma trybu zimowego |
| kąt nachylenia | ~45°, azymut południowy | **nieznany dokładnie** — patrz §7 |

**To nie jest bojler pieca C.O.** (`czujnik_temp_Bojlera`, BUS_2, 0…160 °C). Dwa różne układy.

---

## 4. Równanie kolektora

### 4.1 Postać Hottel–Whillier–Bliss

Standard od 1958 r., podstawa norm EN 12975 i ISO 9806:

```
Q = A × [ η₀·G − a₁·(T − Ta) − a₂·(T − Ta)² ]
```

| symbol | znaczenie | jednostka |
|---|---|---|
| `Q` | moc cieplna oddawana do wody | W |
| `A` | powierzchnia apertury | m² |
| `η₀` | sprawność optyczna (zero strat) | – |
| `G` | **nasłonecznienie w płaszczyźnie kolektora (GTI)** | W/m² |
| `a₁` | współczynnik strat liniowych | W/(m²·°C) |
| `a₂` | współczynnik strat kwadratowych | W/(m²·°C²) |
| `T` | temperatura wody w kolektorze | °C |
| `Ta` | temperatura powietrza | °C |

Moc jest **przycinana do zera** — kolektor nie „grzeje na minus".
Gdy `G = 0` (noc), `Q = 0` i zostaje samo chłodzenie zbiornika.

### 4.2 Postać sprawnościowa (EN 12975)

To samo, podzielone przez `G`:

```
η = η₀ − a₁·(T − Ta)/G − a₂·[(T − Ta)/G]²
```

---

## 5. Parametry

### 5.1 Kolektor

Producent nie jest znany. Dobrałem **Apricus ETC-30** — certyfikat Solar Keymark,
te same wymiary rur (58 × 1800 mm, heat pipe), ta sama klasa cenowa.
Parametry przeliczone na aperturę:

| parametr | wartość | źródło |
|---|---|---|
| `η₀` | **0,714** | Apricus ETC-30, karta techniczna |
| `a₁` | **1,243 W/(m²·°C)** | tamże |
| `a₂` | **0,009 W/(m²·°C²)** | tamże |
| `A` | **0,95 m²** | 10/30 × 2,84 m² |
| `A·η₀` | **0,678 m²** | iloczyn |

**Dla porównania inne rury próżniowe** (żeby wiedzieć, czy 1,243 to sensowna liczba):

| model | η₀ | a₁ |
|---|---|---|
| Apricus ETC-30 | 0,714 | 1,243 |
| Vitosol 300 SP3 | 0,784 | 1,36 |
| rury heat pipe, klimat śródziemny | 0,735 | 1,16 |
| inny ETC (karta DS_879) | 0,51 | 0,94 |

1,243 leży pośrodku. Kolektory płaskie mają `a₁` od 3 do 5 — **rury próżniowe tracą
ciepło 3–4 razy wolniej**, i to jest ich cała zaleta przy ciepłej wodzie.

### 5.2 Zbiornik

| parametr | wartość | źródło |
|---|---|---|
| pojemność | **418,6 kJ/°C** | 100 l × 4,186 kJ/(kg·K) |
| `UA` (strata postojowa) | **1,4 W/°C** | literatura: 1,07–1,46 W/K dla 100–160 l |

Zakres z literatury: badanie TRNSYS podaje `UA = 1,07 W/K` dla zbiornika 160 l,
inne `1,30–1,70 W/K`, jeszcze inne `1,46 W/K`. Przyjęte **1,4** leży w środku.

---

## 6. Najważniejsze odkrycie: GTI, nie GHI

### 6.1 Problem

Aplikacja pobiera `shortwave_radiation` — to **GHI**, słońce na płaszczyznę **poziomą**.
Kolektor stoi pod kątem ~45°, więc dostaje **GTI** (`global_tilted_irradiance`).

To nie jest drobna różnica. Stosunek GTI/GHI zależy od zachmurzenia:

| dzień | GHI | GTI 45° | GTI/GHI |
|---|---|---|---|
| 2026-10-20 pochmurny | 0,50 | **0,44** | **×0,88** |
| 2026-10-21 słoneczny | 2,59 | **4,16** | **×1,61** |
| 2026-10-22 pochmurny | 0,38 | **0,34** | **×0,88** |
| 2026-10-23 słoneczny | 2,66 | **4,87** | **×1,83** |

**Przy pochmurnym niebie pochyła płaszczyzna dostaje MNIEJ niż pozioma.**
Szare niebo świeci równomiernie z całej kopuły; płaszczyzna pozioma widzi całą kopułę,
a pochylona — tylko jej część.

**Przy bezchmurnym dostaje prawie dwa razy więcej.**

Zakres ×0,88 … ×1,83. **Żaden stały mnożnik tego nie zastąpi.**

### 6.2 Rozwiązanie

Open-Meteo liczy GTI. Wystarczy dodać do zapytania:

```
&hourly=global_tilted_irradiance
&tilt=45
&azimuth=0
```

- `tilt` — kąt od poziomu, 0–90°
- `azimuth` — 0 = południe, −90 = wschód, +90 = zachód, ±180 = północ

Odpowiedź zawiera wtedy `global_tilted_irradiance` obok `shortwave_radiation`.

### 6.3 Uczciwa uwaga o dowodzie

Na czterech dniach **statystycznie nie da się udowodnić, że GTI jest lepsze**:

| wejście | R² dopasowania liniowego |
|---|---|
| GTI | 0,979 |
| GHI | 0,988 |

GHI wyszło odrobinę lepiej. To jednak artefakt małej próbki: przy 4 punktach
i 2 parametrach oba modele się wpasowują, a 2026-10-23 miała jednocześnie
więcej słońca i cieplejszą wodę, więc efekty się maskują.

**GTI jest wybrane ze względu na fizykę, nie na R²** — kolektor stoi pod kątem,
więc mierzy się słońce padające pod kątem. GHI to inna wielkość.

Do rozstrzygnięcia na danych potrzeba dni z pogodą zmieniającą się w ciągu doby.

---

## 7. Kąt nachylenia — mało ważny

| dzień | GTI 45° | GTI 60° | różnica |
|---|---|---|---|
| 2026-10-20 | 0,44 | 0,40 | −9,4% |
| 2026-10-21 | 4,16 | 4,27 | +2,6% |
| 2026-10-22 | 0,34 | 0,31 | −8,8% |
| 2026-10-23 | 4,87 | 5,08 | +4,3% |

W październiku między 45° a 60° różnica wynosi 1–9%.
**Kąta nie trzeba mierzyć dokładnie** — wystarczy „mniej więcej 45 stopni, na południe".

Wpływ symulacji zbiornika: maksimum 44,1 °C przy 45° vs 45,1 °C przy 60°.

---

## 8. Równanie zbiornika

Kolektor grzeje, zbiornik traci:

```
C · dT/dt = Q(t) − UA·(T − Ta) − Q_pobór
```

gdzie `C = 418,6 kJ/°C`, `Q` z §4.

To równanie różniczkowe — **nie da się go zastąpić jednym mnożeniem**.
Trzeba liczyć krok po kroku, godzinę po godzinie (w kodzie: 120 podkroków po 30 s).

**Dla porównania:** TRNSYS dzieli zbiornik na 6–100 warstw (Type 60).
Tu wystarczy jedna warstwa, bo jest jeden czujnik w połowie wysokości.

### 8.1 Przesunięcie temperatury wody w kolektorze

Kolektor pobiera wodę z **dołu** zbiornika, a czujnik jest w **połowie**.
Dolna warstwa jest chłodniejsza. W symulacji przyjąłem `−4 °C`, ale przy
parametrach katalogowych lepiej wychodzi `0 °C` — patrz §9.

---

## 9. Sprawdzenie na czterech dniach

### 9.1 Parametry katalogowe, zero dopasowania

```
A·η₀ = 0,678    a₁ = 1,243    a₂ = 0,009    UA = 1,4    offset = 0
```

| dzień | start → koniec | max model | max pomiar | błąd |
|---|---|---|---|---|
| 2026-10-21 | 19,8 → 38,1 °C | 40,0 | 39,2 | **+0,8** |
| 2026-10-23 | 25,8 → 45,1 °C | 47,4 | 44,8 | **+2,6** |
| 2026-10-22 (22:40) | 35,2 → 29,6 °C | — | 29,2 | **+0,4** |

**Średni błąd 1,3 °C.** Model lekko przegrzewa (wszystkie błędy dodatnie),
ale poprawia się przy pochmurnym dniu — czyli tam, gdzie aplikacja była najgorsza.

### 9.2 Porównanie z tym, co jest teraz

| dzień | model aplikacji (GHI × 4,2) | błąd | model fizyczny | błąd |
|---|---|---|---|---|
| 2026-10-21 | +10,4 °C | **−3,4** | +0,8 °C | **+0,8** |
| 2026-10-23 | +11,1 °C | **−4,1** | +2,6 °C | **+2,6** |
| 2026-10-22 | +0,8 °C | **+6,6** | +0,4 °C | **+0,4** |

Aplikacja myli się o −4,1 i +6,6. Model o +2,6 i +0,4.
**I model poprawnie przewiduje spadek temperatury**, czego aplikacja nie potrafi wcale.

### 9.3 Parametry dopasowane do instalacji

Kazałem programowi znaleźć najlepsze parametry dla tych czterech dni:

| parametr | dopasowane | katalog | różnica |
|---|---|---|---|
| `A·η₀` | 0,650 m² | 0,678 | −4% |
| `UA` | 1,4 W/°C | 1,07–1,46 | w zakresie |
| `a₁` | 2,4 W/(m²·°C) | 1,243 | 2× wyżej |
| offset | −4 °C | — | dolna warstwa |

Błąd spada do ±0,5 °C, **ale cztery parametry na trzy punkty pomiarowe to przeuczenie.**
Dlatego domyślnie używamy wartości katalogowych.

**Niespójność:** dopasowane `a₁` jest dwa razy wyższe niż katalogowe. Prawdopodobne
przyczyny: opór cieplny heat pipe (nie ma go w równaniu HWB), nocny przepływ
wsteczny w termosyfonie (literatura: może zwiększyć straty czterokrotnie),
szczelina między czujnikiem a wodą. **Do rozstrzygnięcia po 2–3 tygodniach danych.**

---

## 10. Energia

Bieg od zmierzonego minimum do końca doby (§9.1 — ten sam przebieg, nie osobne obliczenie):

| dzień | GTI | energia (model) | szczyt | przyrost od startu |
|---|---|---|---|---|
| 2026-10-21 | 4,16 kWh/m² | **2,62 kWh** | **40,0 °C @ 18:00** | +20,2 °C |
| 2026-10-23 | 4,87 kWh/m² | **2,91 kWh** | **47,4 °C @ 18:00** | +21,6 °C |

*(szacunek modelu, nie pomiar — jeden czujnik w połowie zbiornika nie widzi rozkładu)*

Szczyt zawsze ok. 18:00, nie 13:00. Woda ma bezwładność — słońce już słabnie,
a zbiornik jeszcze się grzeje.

### 10.1 Wpływ temperatury wody — z pomiarów, nie z modelu

To jest odpowiedź na pytanie „im zimniejsza woda, tym szybciej się nagrzewa".

Dwa dni słoneczne o zbliżonym nasłonecznieniu:

| dzień | GTI | woda | amplituda (max−min) | K na kWh/m² |
|---|---|---|---|---|
| 2026-10-21 | 4,16 | 19,8 → 39,2 | 19,4 K | **4,66** |
| 2026-10-23 | 4,87 | 25,8 → 44,8 | 19,0 K | **3,90** |

2026-10-23 miała **o 17% więcej słońca** i **o 5,8 °C cieplejszą wodę** —
a amplituda wyszła **o 2% mniejsza**.

Na każdą kWh/m² 2026-10-23 dała **o 16% mniej** niż 2026-10-21.

**To jest cała różnica.** Gdyby 2026-10-21 dostała GTI 4,87, amplituda
wyniosłaby ok. 22,7 K. Dostała 19,0 K — o 3,7 K mniej, przy wodzie
cieplejszej o 5,8 °C.

Dwa mechanizmy działają naraz:

1. **Strata do otoczenia rośnie z temperaturą wody.** Woda o 45 °C traci
   ciepło szybciej niż woda o 20 °C — przy tym samym powietrzu.
2. **Kolektor grzeje tym gorzej, im cieplejsza woda weń wchodzi.**
   To widać wprost we wzorze: `− a₁·(T − Ta)`. Im większe `T`, tym mniejszy `Q`.

Uwaga na sformułowanie: **szybkość grzania w °C/s jest stała** — ile energii
potrzeba na 1 °C zależy tylko od ilości wody. Ale przy zimnej wodzie **mniej
ciepła ucieka**, więc zostaje go więcej. Efekt jest ten sam, mechanizm inny.

**Ostrzeżenie:** dwa dni to nie jest dowód. Zimna woda rano pojawia się po
bezchmurnej nocy, a bezchmurna noc zwiastuje słoneczny dzień — więc „zimna woda"
i „dużo słońca" chodzą razem z tej samej przyczyny. Przy dopasowaniu trzeba
rozdzielić GTI od temperatury startowej, a nie patrzeć na samą korelację.


---

## 11. Prosty wzór (do szybkiego szacunku)

Gdy nie chcesz liczyć godzinnie:

```
przyrost[°C] = 4,43 × GTI[kWh/m²/dobę] − 5,57
```

| dzień | GTI | model | pomiar | błąd |
|---|---|---|---|---|
| 2026-10-20 | 0,44 | −3,62 | −2,0 | −1,62 |
| 2026-10-21 | 4,16 | +12,86 | +13,8 | −0,94 |
| 2026-10-22 | 0,34 | −4,06 | −5,8 | +1,74 |
| 2026-10-23 | 4,87 | +16,00 | +15,2 | +0,80 |

**Średni błąd 1,3 °C.** Próg równowagi: **1,26 kWh/m²** — poniżej tej dobowej
sumy zbiornik się wychładza.

### 11.1 Zaskakujące podobieństwo

Stała w aplikacji to **4,2**, a tutaj wychodzi **4,43**. Prawie to samo.

Czyli współczynnik 4,2 był policzony poprawnie — **ale dla GTI**.
Aplikacja karmi go GHI. Stąd cały błąd.

Brakuje tylko drugiego członu `−5,57`, czyli dobowej straty.
Bez niego model nie potrafi pokazać spadku temperatury.

---

## 12. Algorytm

```
WEJŚCIE:
  GTI[h]   — nasłonecznienie w płaszczyźnie kolektora, W/m², 24 wartości
  Ta[h]    — temperatura powietrza, °C, 24 wartości
  T0       — temperatura wody o północy, °C (z czujnika)
  A_eta0 = 0.678, a1 = 1.243, a2 = 0.009, UA = 1.4, C = 418600 J/K

T ← T0
energia ← 0
dla każdej godziny h = 0..23:
    dla 120 podkroków po 30 s:
        Q ← A_eta0·GTI[h] − a1·(T − Ta[h]) − a2·(T − Ta[h])²
        jeśli Q < 0: Q ← 0                    # noc = brak grzania
        energia ← energia + Q·30
        T ← T + (Q − UA·(T − Ta[h]))·30 / C
        zapamiętaj szczyt

WYJŚCIE:
  T_koniec, T_szczyt + godzina, przyrost = T_szczyt − T0, energia/3.6e6 kWh
```

**Uwaga:** przy `GTI = 0` (noc) `Q = 0`, więc zostaje `T ← T − UA·(T−Ta)·dt/C`.
To daje wychładzanie. Sprawdzone na 2026-10-22: symulacja 29,6 °C, pomiar 29,2 °C.

---

## 13. Czego model NIE wie

| obszar | stan |
|---|---|
| azymut kolektora | założone południe; odchyłka 30° zmieni wynik o kilka % |
| kąt nachylenia | założone 45°; między 45° a 60° różnica ≤9% |
| straty na rurach | **pominięte** — mogą zjadać 0,2–0,5 kWh/dobę |
| stratyfikacja | jedna warstwa; czujnik w połowie wysokości |
| pobór ciepłej wody | **brak** w modelu — a to realnie obniża temperaturę |
| nocny przepływ wsteczny | nie modelowany; literatura mówi o 4× większych stratach |
| tryb zimowy | brak; zimą kolektor jest opróżniony |
| szron/śnieg na rurach | brak |
| producent kolektora | nieznany — parametry z analogicznego modelu |
| **liczba dni walidacji** | **4** — to za mało na pewne parametry |

Model jest **lepszy od obecnego**, ale **nie jest jeszcze wiarygodny ilościowo**.
Po 2–3 tygodniach będzie można dopasować `a₁` i offset na prawdziwych danych.

---

## 14. Scalenie z aplikacją

### 14.0 WAŻNE: drzewo Androida w repo jest starsze niż telefon

| miejsce | wersja |
|---|---|
| drzewo w repozytorium | **v0.31.32** (po commicie z 2026-10-24) |
| aplikacja na telefonie | **v0.31.31** (versionCode 3037), v0.31.32 do wgrania |

**Stan na 2026-10-24:** drzewo w repo dogoniło paczki — pliki
`SolarThermalModel.kt`, `SolarAnalytics.kt`, `Weather.kt`, `Sensors.kt`
są teraz w repozytorium w wersji v0.31.32. Wcześniej repo stało na
v0.31.30, a v0.31.30 i v0.31.31 szły do projektu tylko jako paczki
w `android-sync/`. `app/build.gradle.kts` w repo ma nadal stare
`versionCode 3017` — podbija się go lokalnie przed budowaniem APK.

**Co z tego wynika dla tego modelu:**

Plik `SolarThermalModel.kt` jest **samodzielny** — nie zależy od żadnej innej
klasy aplikacji poza `WHour` (i to tylko w jednej funkcji `fromWeather`).
Można go skopiować do drzewa v0.31.28 i zadziała bez zmian.

Zmiany w `Weather.kt` (§14.1–14.2) trzeba nanieść ręcznie na drzewo v0.31.28,
bo plik różni się między wersjami.

### 14.1 Co zmienić w zapytaniu do pogody

`android/.../core/Weather.kt`, funkcja `fetch()`:

```kotlin
// przed:
"...&hourly=...,uv_index,is_day,shortwave_radiation" +

// po:
"...&hourly=...,uv_index,is_day,shortwave_radiation,global_tilted_irradiance" +
"&tilt=45&azimuth=0" +
```

### 14.2 Co dodać do modelu godziny

`WHour` — nowe pole **z wartością domyślną**, więc stare wywołania się nie psują:

```kotlin
val tiltedIrradiance: Double? = null
```

Parsowanie — analogicznie do `shortwaveRadiation`:

```kotlin
tiltedIrradiance = arrOrNull(hh, "global_tilted_irradiance", i)
```

### 14.3 Co podmienić w liczeniu zysku

```kotlin
// przed (SolarAnalytics):
gain = max(0, deltaP) + max(0, deltaB)   // sumuje drgnienia
forecastGain = kwhM2 * 4.2               // mnoży GHI

// po:
val wynik = SolarThermalModel.simulate(tStartC, godziny, SolarThermalModel.Params())
val prognoza = wynik.deltaC              // przyrost od startu do szczytu
val energia  = wynik.energyKwh
val szczyt   = wynik.peakTempC
```

`tStartC` — temperatura wody o północy, z `czujnik_temp_Panel_Plus`.

### 14.4 Co pokazać na ekranie

Zamiast „ZYSK DZIŚ +21,5 °C" (nieprawdziwe) proponuję trzy liczby:

```
Woda teraz                  33,2 °C
Prognoza na dziś            +19,4 °C   (szczyt 45,2 °C ok. 18:00)
Energia zbierana dziś        2,93 kWh   (szacunek)
```

Każda z etykietą „zmierzone" albo „szacunek".

---

## 15. Źródła

**Równanie kolektora**
- Hottel–Whillier–Bliss, Duffie & Beckman, *Solar Engineering of Thermal Processes* — [Wykład LTH](https://www.lth.se/fileadmin/ht/Kurser/MVK160/Project_08/Fabio.pdf)
- [EnergyPlus Engineering Reference — Solar Collectors](https://bigladdersoftware.com/epx/docs/24-2/engineering-reference/solar-collectors.html)

**Parametry kolektora (norma EN 12975 / ISO 9806)**
- [Apricus ETC-30, karta techniczna](https://www.apricus.com/upload/userfiles/downloads/ETC-30_Tech_Info_Sheet_Int.pdf) — η₀ = 0,714, a₁ = 1,243, a₂ = 0,009
- [Polysun — model kolektora wg EN 12975](https://www.velasolaris.com/en/handbuch/polysun-designer/producers/solar-thermal-collectors/collector-model-according-to-european-standards-en/) — zakresy dla rur: η₀ 0,65–0,80, a₁ 1–2
- [Vitosol 300 SP3](http://www.sdewes.org/jsdewes/pid7.0285) — η₀ = 0,784, a₁ = 1,36

**Zbiornik**
- [TRNSYS, porównanie RETScreen/TSOL/TRNSYS](https://www.sciencedirect.com/science/article/pii/S2590123026025624) — UA = 1,07 W/K dla 160 l
- [IEA SHC Task 44 — modele zbiorników](https://task44.iea-shc.org/Data/Sites/1/publications/T44A38_Rep_C2_E_Storage_Final_Draft.pdf) — Type 60, 6–100 warstw

**Termosyfon i przepływ wsteczny**
- [OSTI — nighttime performance of thermosyphon](https://www.osti.gov/biblio/116242) — przepływ wsteczny zwiększa straty niemal 4×

**GTI**
- [Open-Meteo — dokumentacja](https://open-meteo.com/en/docs) — `global_tilted_irradiance`, parametry `tilt` i `azimuth`

**IAM (nie użyte, ale istotne dla rur próżniowych)**
- [ESTIF — testowanie kolektorów rurowych](http://www.estif.org/solarkeymarknew/images/downloads/QAiST/qaist%20d2.1%20r2.1%20performance%20testing%20of%20evacuated%20tubular%20collectors.pdf) — IAM poprzeczny ≤1,10, podłużny 0,88

---

## 16. Dane użyte do walidacji

Pobrane z Open-Meteo dla `51.066389, 21.509167`, `tilt=45`, `azimuth=0`,
strefa `Europe/Warsaw`.

| dzień | GTI [kWh/m²] | Ta średnia [°C] | T start [°C] | T max pomiar | godzina |
|---|---|---|---|---|---|
| 2026-10-20 | 0,44 | 10,3 | 21,2 | 25,0 | — |
| 2026-10-21 | 4,16 | 13,1 | 19,8 | 39,2 | 15:23 |
| 2026-10-22 | 0,34 | 11,3 | 29,2 | 35,2 | 00:14 |
| 2026-10-23 | 4,87 | 9,6 | 25,8 | 44,8 | 14:06 |

Pomiary z archiwum centrali (`SolarDailyArchive`), kanał `TELEMETRY_CH_PANEL`.

---

## 17. Historia

| data | co |
|---|---|
| 2026-10-23 | pierwsza wersja; model sprawdzony na 4 dniach; **nie wgrany do aplikacji** |
| 2026-10-23 | sprostowanie: obliczenia wykonane dla Radomia, poprawione na Trzemchę Górną |
| 2026-10-23 | `SolarThermalModel.kt` wgrany do aplikacji (v0.31.29) — model jednowęzłowy |
| 2026-10-24 | **model dwuwęzłowy z pętlą kolektora** (§18), v0.31.32: RMS 5,85 → 2,63 °C |

**Następne kroki po 2–3 tygodniach zbierania danych:**
1. Dopasować `a₁` i offset na większej próbce.
2. Sprawdzić, czy `UA` wychodzi stale ok. 1,4 W/°C.
3. Dodać pobór wody (jest już wykrywany przez `SolarAnalytics`).
4. Rozważyć dodanie strat na rurach.

---

## 18. Model dwuwęzłowy — pętla kolektora (v0.31.32, 2026-10-24)

### 18.1 Dlaczego jeden węzeł nie wystarczał

Model z §1–§13 ma **jeden węzeł**: wodę w zbiorniku. Kolektor jest w nim
tylko źródłem ciepła w dzień. Nocą nie robi nic.

W rzeczywistej instalacji termosyfonowej kolektor działa także nocą —
inaczej niż zakładał model. Woda w kolektorze stygnie szybciej niż
w zbiorniku (mniejsza pojemność, większe straty na jednostkę objętości),
robi się gęstsza i spływa w dół rurą powrotną, wypychając cieplejszą wodę
ze zbiornika w górę. Napędza to sama różnica gęstości — bez pompy.
Zjawisko nazywa się **cyrkulacją wsteczną termosyfonu** (reverse circulation).

### 18.2 Ile to zabiera — policzone z archiwum

Dla każdej nocy osobno: spadek temperatury w zbiorniku × pojemność cieplna
100 l, podzielone przez (średnia różnica zbiornik−powietrze) × godziny.
Wynik to **efektywne UA** tej nocy.

| odcinek | spadek | godzin | strata rzeczywista |
|---|---|---|---|
| 21.10 wieczór | 4,0 K | 8,8 | 2,3 W/K |
| 22.10 (dzień pochmurny) | 6,0 K | 22,4 | 1,5 W/K |
| 22.10 noc | 3,4 K | 8,3 | 2,3 W/K |
| 23.10 wieczór (słoneczny) | 11,4 K | 8,9 | 4,6 W/K |
| 24.10 noc (po słonecznym) | 8,8 K | 9,3 | 4,3 W/K |

Po dniu pochmurnym strata zgadza się z katalogiem (1,5 ≈ 1,4 W/K).
Po dniu słonecznym jest **trzy razy większa**. Ta różnica to cyrkulacja
wsteczna: po słonecznym dniu cała pętla jest gorąca, więc różnica
temperatur pętla−zbiornik jest duża i przepływ wsteczny silny.

Wniosek: strat nie da się oddać jednym mnożnikiem `UA`. Potrzebny jest
drugi węzeł.

### 18.3 Równania

Dwa węzły: woda w pętli (kolektor + manifold + rury) i zbiornik 100 l.
Krok `dt = 30 s`, jak w §12.

```
q      = η₀·G·A − a₁·(T_loop − T_air) − a₂·G·(T_loop − T_air)²     [W], przycięte do ≥ 0
dT_loop = (q − UA_loop·(T_loop − T_air))·dt / C_loop
flow    = FORWARD  gdy T_loop > T_tank
        = reverseFlowKgS  w przeciwnym razie
q_flow  = flow·c_w·(T_loop − T_tank)                                [W]
dT_tank = (q_flow − UA_tank·(T_tank − T_air))·dt / C_tank
T_loop -= q_flow·dt / C_loop
```

Kierunek przepływu wynika z różnicy temperatur — nie ma przełącznika
„dzień/noc”. To jest cała różnica względem modelu jednowęzłowego.

### 18.4 Parametry

| parametr | wartość | skąd |
|---|---|---|
| `FORWARD_FLOW_KG_S` | 0,05 kg/s | termosyfon słoneczny, rząd wielkości ~2–4 l/min |
| `reverseFlowKgS` | **0,004 kg/s** | OSTI biblio/116242: ~0,005 kg/s; heat-pipe mniej (IJLCT 10(4) 2015) |
| `loopUaWk` | 1,0 W/K | manifold + odkryte rury |
| `loopWaterKg` | 4,0 kg | 10 rur + manifold + rury |
| `tankUa` | 1,4 W/K | bez zmian, §7 |
| `C_tank` | 418 600 J/K | 100 l × 4186 J/(kg·K) |
| `C_loop` | 16 744 J/K | 4 kg × 4186 J/(kg·K) |

### 18.5 Wynik — 5 dób, symulacja ciągła

Start 20.10 godz. 00:00, T₀ = 24 °C, temperatury powietrza z Open-Meteo,
GTI w płaszczyźnie 45°, porównanie z min/max z archiwum (10 punktów):

| model | RMS | obciążenie min | obciążenie max |
|---|---|---|---|
| jednowęzłowy (v0.31.31) | 5,85 °C | +4,4 °C | +3,1 °C |
| **dwuwęzłowy (v0.31.32)** | **2,63 °C** | **−0,0 °C** | **−0,5 °C** |

Stary model był **stale za ciepły** — na każdej dobie, o 3–5 °C.
Nowy trafia raz w górę, raz w dół.

Szczyt 23.10:

| | temperatura | godzina | błąd godziny |
|---|---|---|---|
| zmierzone | 47,2 °C | 15:05 | — |
| jednowęzłowy | 46,6 °C | 18:00 | +2,92 h |
| dwuwęzłowy | 44,7 °C | 17:01 | +1,95 h |

### 18.6 Czego ten model nadal nie umie

**Godzina szczytu jest ~2 h za późno.** To nie jest błąd rachunku, tylko
konsekwencja traktowania zbiornika jako jednej bryły.

Prawdziwy zbiornik 100 l jest uwarstwiony: góra cieplejsza, dół zimniejszy,
czujnik w połowie wysokości. Model uśrednia całe 100 l, więc woda
„dogrzewa się” do wieczora, gdy w rzeczywistości górna warstwa osiągnęła
szczyt już wczesnym popołudniem.

Naprawa = model warstwowy (kilka warstw zbiornika z mieszaniem).
To osobna, większa zmiana — nie w tej wersji.

Parametry pętli sprawdzone na 5 dobach. Po 2–3 tygodniach danych warto
powtórzyć dopasowanie.

### 18.7 Źródła

- OSTI `biblio/116242` — *Performance analysis of a thermosyphon solar water
  heater, Part 2: Nighttime performance*: przepływ wsteczny ~0,005 kg/s,
  straty większe „nearly fourfold”.
- Morrison G.L. i in., *Solar Energy* 1986, `0038092X86901556` — separacja
  kolektor–zbiornik ≥ 200 mm (typowo 500 mm), żeby ograniczyć cyrkulację wsteczną.
- *Int. J. Low-Carbon Technologies* 10(4) 2015, `10/4/430` — kolektor
  heat-pipe: przepływ wsteczny mniejszy niż w płaskim; spadek nocny 2–10 °C.
- Michaelides (EuroSun 2010) — twierdzi wpływ wsteczki „minimalny”;
  **sprzeczne** z OSTI. Dlatego model dwuwęzłowy, a nie mnożnik na `UA`.

---

## 19. Kalibracja z archiwum — sprawdzona i odrzucona (2026-10-29)

Sekcja 18 kończyła się zdaniem „po 2–3 tygodniach danych warto powtórzyć
dopasowanie". Dane są — 11 pełnych nocy 18–28.10 — więc dopasowanie zostało
zrobione. **Wynik jest negatywny i to też jest wynik.**

### 19.1 Ranga parametrów — co w ogóle da się ruszyć

| parametr | z 1,0 → | reakcja | ocena |
|---|---|---|---|
| `reverseFlowKgS` | 0,002 → 0,050 (25×) | 0,02 °C/h | **bezwładny** — nic nie zmienia |
| `loopUaWk` | 1,0 → 5,0 | noc −1,26 °C ✅, ale dzień 33,3 → 26,9 (−19%) ❌ | nie da się |
| `loopUaWk` + `apertureOptical` | 5,0 + 0,85 | dzień 34,2 ✅, ale η₀ = 0,89 | **fizycznie niemożliwe** |
| `tankUa` | 1,4 | na nocach z ΔT 12–19 K wychodzi 1,2–1,4 W/K | **potwierdzone, nie ruszać** |

Ograniczeniem nocnym jest `loopUaWk`, nie przepływ. Podniesienie go psuje dzień.
Podniesienie obu naraz wypycha sprawność optyczną ponad granicę fizyki.

### 19.2 Cztery metody uczenia na własnym archiwum

Każda uczy się na 10 nocach, sprawdzana na jedenastej (leave-one-out), 11 nocy:

| metoda | średni błąd |
|---|---|
| **model fizyczny (0.31.51)** | **2,05 °C** |
| uczony współczynnik `k` zamiast modelu | 2,89 °C |
| persystencja (tempo z pierwszych 2 h) | 1,95 °C |
| model × mnożnik uczony na pozostałych | 2,28 °C |
| model + stała poprawka uczona na pozostałych | 2,10 °C |

Na teście praktycznym „ile zostanie do 24:00" (66 przypadków, godziny 17:00–22:00)
persystencja przegrywa zdecydowanie: **2,00 °C vs 1,29 °C** — bo o 17:00–18:00
jeszcze grzeje słońce, a persystencja o tym nie wie.

Skrypt odtwarzalny: `narzedzia/symulator-solar-2026-10-29/kalibracja/porownanie.py`.

### 19.3 Dlaczego uczenie tu nie może działać

Błąd modelu nie zależy od niczego mierzalnego:

| z czym porównane | korelacja |
|---|---|
| temperatura wody o 19:00 | +0,33 |
| temperatura powietrza | −0,31 |
| różnica woda − powietrze | +0,24 |

Przy braku korelacji jedyne, co robi uczenie, to wpisuje na stałe średnią
pomyłkę — psując noce, które wcześniej wychodziły dobrze.

### 19.4 Cały błąd siedzi w jednej nocy

Mediana błędu bezwzględnego: **1,40 °C**. Bez nocy 23/24.10 średnia spada
z 2,05 do **1,45 °C** — ta jedna noc to 36% sumy błędu.

23/24.10: zbiornik 42,5 → 27,8 °C, bez skoków, bez wykrytego poboru, ~1,5 °C/h
przy ΔT ≈ 30 K → **UA_eff ≈ 6,1 W/K** (4,4× katalog).

**KOREKTA tego samego dnia (zgłoszenie użytkownika: „nic nie kapie").**
Pierwsza wersja tej sekcji twierdziła, że ubytek odpowiada stałemu dopływowi
4,4 l/h, czyli że gdzieś cieknie kran. Wniosek wycofany — opierał się na tym,
że tylko ta jedna noc odstawała od katalogu. Po policzeniu UA_eff **regresją**
(każda noc osobno, nie z dwóch punktów) okazało się, że odstawanie jest regułą,
a nie wyjątkiem:

| noc | T o 19:00 | spadek | UA_eff [W/K] | × katalog (1,4) |
|---|---|---|---|---|
| 18/19.10 | 28,0 | −2,0 | 1,33 | 0,95 |
| 19/20.10 | 24,2 | −1,4 | 1,20 | 0,86 |
| 20/21.10 | 23,2 | −1,7 | 1,53 | 1,09 |
| 21/22.10 | 36,5 | −2,5 | 1,38 | 0,99 |
| 22/23.10 | 30,8 | −4,6 | 2,88 | 2,06 |
| **23/24.10** | 42,8 | **−14,3** | **6,14** | **4,38** |
| 24/25.10 | 21,8 | −4,3 | 4,63 | 3,31 |
| 25/26.10 | 20,0 | −4,5 | **12,27** | **8,77** |
| 26/27.10 | 14,8 | −3,0 | 4,88 | 3,48 |
| 27/28.10 | 22,5 | −5,5 | 7,19 | 5,13 |
| 28/29.10 | 17,5 | −1,0 | 3,52 | 2,51 |

Czyli **tylko 4 noce z 11 zgadzają się z katalogiem**, a rozrzut wynosi 1,2–12,3 W/K,
czyli dziesięciokrotnie. Zdanie „tankUa = 1.4 potwierdzone pomiarem" też było
za mocne — opierało się na tej samej zgrubnej metodzie. `tankUa = 1.4` to nadal
najlepszy **pojedynczy** parametr (bo taki jest katalog tego zbiornika), ale
**nie jest to prawda o każdej nocy**.

Co z tego wynika dla modelu: skoro efektywna strata zmienia się dziesięciokrotnie,
to **żadna stała nie opisze wszystkich nocy** — ani 1,4, ani nauczona z archiwum.
To domyka wątek kalibracji: nie ma czego dopasowywać, bo w każdej nocy działa
inny mechanizm.

Czego NIE wiadomo: co te noce różni. Sprawdzone i wykluczone:
- **wyciek** — użytkownik: „nic nie kapie",
- **pojedynczy pobór wody** — na 23/24.10 spadek był gładki, −0,2…−0,3 °C co 5 min,
  żadnego skoku; przy poborze 40 l byłby skok ~4 K w jednej próbce,
- **pompa** — na 23/24.10 `pompa=OFF` przez całą noc (0/132 próbek ON),
- **błąd czujnika** — w całym logu jest **jedna** próbka −1,0 °C (24.10 13:47,
  zaraz po restarcie) i nie trafiła w żadne okno nocne.

Nie wykluczone, w kolejności prawdopodobieństwa:
1. **wiatr** — zbiornik stoi na dachu; aplikacja **już pobiera** `wind_speed_10m`
   (`Weather.kt`), ale model cieplny tej liczby nigdzie nie używa,
2. **cyrkulacja wody użytkowej** (jeśli jest) — ciągły, mały przepływ, którego
   nie widać jako skok,
3. **cyrkulacja wsteczna pętli solarnej** — literatura (OSTI 116242) mówi
   „nearly fourfold", zmierzone 4,4×, ale to nie tłumaczy nocy 25/26.10 (8,8×)
   przy zbiorniku zaledwie 20 °C, czyli bez ciągu termicznego.

### 19.5 Decyzja

`MODEL_SOLARNY` **zostaje bez zmian**. Wersja 0.31.51 zostaje wgrana.
Kalibracja z archiwum **nie wchodzi do kodu** — nie z powodu ostrożności,
ale dlatego, że cztery niezależne sposoby uczenia wypadły gorzej niż model.

Jeżeli kiedyś uda się zmierzyć, co różni te noce (patrz wyżej), temat można wrócić.

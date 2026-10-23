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
| drzewo w repozytorium | **v0.31.11** (versionCode 3017) |
| aplikacja na telefonie | **v0.31.28** (versionCode 3034) |

Zmiany v0.31.12 … v0.31.28 istnieją w repo **tylko jako pliki `.diff`**
w katalogu `android-sync/`. Z repozytorium nie da się zbudować APK, który
masz na telefonie.

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

**Następne kroki po 2–3 tygodniach zbierania danych:**
1. Dopasować `a₁` i offset na większej próbce.
2. Sprawdzić, czy `UA` wychodzi stale ok. 1,4 W/°C.
3. Dodać pobór wody (jest już wykrywany przez `SolarAnalytics`).
4. Rozważyć dodanie strat na rurach.

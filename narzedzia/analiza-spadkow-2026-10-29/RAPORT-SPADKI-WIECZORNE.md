# Spadki temperatury w zbiorniku wieczorami — pobór wody czy wychładzanie?

Data: 2026-10-29. Dane: eksport RTDB `akwarium-367be-default-rtdb-10-export (1).json`
(01.10 01:00 → 29.10 10:51), 40 789 próbek minutowych.

Pytanie użytkownika: *„powtarzalność spadku temperatury w tym zbiorniku wieczorami
w zależności od temperatury zewnętrznej — to może nie być pobór, tylko ochłodzenie"*.

---

## 1. Skąd wiadomo, która kolumna to woda w zbiorniku

Nie zgadywane. Sprawdzone na archiwum solarnym z centrali (`/archiwum_solarne`):

| dzień | archiwum Telegram | kolumna 7 `hist_short` |
|---|---|---|
| 28.10 | min 16.8 @ 07:20 | 16.8 @ 07:20 ✔ |
| 28.10 | max 20.5 @ 11:49 | 20.5 @ 11:49 ✔ |

Kolumna 7 = `a[5]` = `czujnik_temp_Panel_Plus` = woda w zbiorniku 100 l.
Kolumna 2 = `a[0]` = temperatura powietrza.

## 2. Jakość danych — 376 próbek odrzuconych

- **376 próbek** z `a[5]` = 0,0 °C. To brak odczytu, nie temperatura. Odrzucone.
- **03.10 godz. 06:00** pokazywało 0,0 °C — bez czyszczenia wyszłoby „spadek 34,3 °C",
  czyli fałszywy rekord. Po oczyszczeniu: 21,8 °C.
- Rozdzielczość czujnika **0,1 °C** przy próbce co minutę ⇒ **regresja minutowa jest
  bezużyteczna** (dT/dt to szum kwantyzacji). Dlatego wszystkie wnioski liczone na
  agregatach 4- i 8-godzinnych, nie na minutach.

## 3. Wynik główny: to NIE jest pobór wody

Test: skok ≥ 0,4 °C między kolejnymi próbkami (≤3 min) = sygnatura poboru.

| | liczba nocy |
|---|---|
| wszystkich nocy z danymi | 28 |
| **bez ani jednego skoku** | **26** |
| ze skokami (pobór) | 2 (03.10 — 241 skoków, 25.10 — 35 skoków) |

**26 z 28 nocy nie ma śladu poboru wody.** A spadki tam są — mediana **2,3 °C
w godzinach 18–22** i **3,3 °C w 22–06**. Czyli hipoteza użytkownika jest trafna:
to przeważnie **wychładzanie**, nie pobór.

Dowód dodatkowy — noc 16.10, największy spadek (12,2 °C w 4 h), **zero skoków**.
Co minutę, gładko:

```
18:00 38.5   18:10 36.8   18:20 35.0   18:30 33.5   18:40 32.3
```
Tempo **0,25 °C/min = 15 °C/h**, powietrze stałe 11,5 °C. Żadnego schodka.
Taki przebieg wyklucza pobór wody — pobór daje schodek, nie równię pochyłą.

## 4. Ale temperatura powietrza tego NIE tłumaczy

Regresja po 26 nocach bez poboru: `spadek/h = k · (T_woda − T_powietrze)`

```
k  = 0,0250 /h        R² = −0,047
```

**R² ujemne** = różnica temperatur nie ma żadnej mocy tłumaczącej. Przykłady wprost:

| noc | śr. różnica T−Ta | tempo spadku |
|---|---|---|
| 02.10 | 26,2 K | 1,49 °C/h |
| 11.10 | 24,1 K | 0,54 °C/h |
| 16.10 | 14,9 K | **1,85 °C/h** |
| 18.10 | 16,1 K | 0,18 °C/h |

Większa różnica temperatur potrafi dać **mniejszy** spadek. Gdyby to było zwykłe
przewodzenie przez ścianki zbiornika, tak być nie może.

## 5. Ile strat wynika z samych ścianek — a ile z reszty

Pojemność: 100 l × 4,186 kJ/(kg·K) = **418,6 kJ/K** (0,1163 kWh/K).

Model z `MODEL_SOLARNY_TRZEMCHA` przyjmuje `UA = 1,4 W/K`. Wtedy:

```
przy różnicy 10 K  -> 0,12 °C/h
przy różnicy 20 K  -> 0,24 °C/h
przy różnicy 30 K  -> 0,36 °C/h
```

Pomiar (mediana nocy bez poboru):

| faza | tempo | UA wyliczone z pomiaru |
|---|---|---|
| 18–22 (pętla ciepła) | 0,57 °C/h | 3,6 W/K |
| 22–06 (pętla zimna) | 0,42 °C/h | 2,9 W/K |

**Nawet w „zimnej" fazie nocy pomiar daje 2,1× więcej strat niż `UA = 1,4 W/K`.**
Wniosek: albo izolacja zbiornika jest gorsza niż katalog, albo dochodzi druga droga
ucieczki ciepła — pętla kolektora.

## 6. Rozkład dwumodalny — pętla „włącza się" albo „nie"

`UA` policzone osobno dla każdej nocy, faza 18–22:

```
1,1  1,5  1,7  1,7  1,8  2,0  2,1  2,2  2,2  2,5  2,9  2,9  3,3  3,3  3,8
5,7  5,8  6,3  6,3  7,3  8,8  9,7  9,8
13,9  18,5  18,8  19,4  27,6
```

Nie rozkład ciągły — **dwie grupy**. Większość nocy: 1–4 W/K. Kilka nocy: 14–28 W/K,
czyli **5–9× więcej**. To sygnatura mechanizmu włącz/wyłącz, nie płynnego przewodzenia.

Sprawdzenie na 7 dniach, dla których jest GHI z archiwum centrali:

| noc | GHI dnia | UA wieczorne |
|---|---|---|
| 25.10 | 2,13 | 19,4 |
| 27.10 | 2,00 | 13,9 |
| 24.10 | 1,83 | 9,7 |
| 23.10 | 2,65 | 6,3 |
| 26.10 | 0,53 | 5,7 |
| 28.10 | 0,48 | 2,2 |
| 29.10 | 0,27 | 2,2 |

Słoneczny dzień → następna noc z dużymi stratami. Pochmurny dzień → małe.
**7 punktów to za mało na dowód**, ale kierunek jest jednoznaczny i zgadza się
z fizyką: po słonecznym dniu kolektor jest gorący i pętla termosyfonowa ma czym
„pompować" ciepło w nocne niebo; po pochmurnym dniu kolektor jest zimny i nic nie krąży.

## 7. Kontrola źródeł z plików MD — co się potwierdza, a co nie

### 7.1 OSTI 116242 — POTWIERDZONE, ale z zastrzeżeniem

Źródło istnieje: *Performance analysis of a thermosyphon solar water heating system.
Part 2: Nighttime performance*, Ghoneim & Nassr, ASME 1995, OSTI ID 116242.
Cytat: „Deduced nighttime mass flow rates are in the order of **0.005 kg/sec**.
The effect of reverse flow was found to increase the system thermal losses by
**nearly fourfold**."

**Zastrzeżenie, którego MD nie podaje:** to instalacja z kolektorem **płaskim**
(1,8 × 0,8 m) i zbiornikiem **100 l**. Nasza instalacja ma kolektor **heat-pipe**
(10 rur próżniowych). MD przenosi „czterokrotnie" z płaskiego na heat-pipe bez
zaznaczenia tego — a to nie to samo, patrz 7.2.

### 7.2 Int. J. Low-Carbon Tech. 10(4) 2015, 430–437 — POTWIERDZONE i pasuje do naszych danych

Ong i in., *Reverse flow in natural convection heat pipe solar water heater*,
DOI 10.1093/ijlct/ctu017. Kluczowe zdania:

- „reverse flow always occurred in the evenings and was about **5–11 times less than
  forward flow**"
- „overnight mean water temperature drop was independent of the height... and ranged
  between **2 and 10 °C**"
- „The temperature drop was greater when the night was cooler."
- **„Nocturnal reverse flow in heat pipe collectors is LESS than that occurring in flat
  plate ones because the condenser section of the former is well insulated"**

Nasza mediana spadku nocnego: **5,6 °C** (2,3 + 3,3). Mieści się dokładnie w 2–10 °C
podanych przez to źródło. To mocne potwierdzenie, że instalacja zachowuje się normalnie.

Ale to samo źródło **osłabia** tezę o „czterokrotności": dla heat-pipe przepływ wsteczny
jest mniejszy niż dla płaskiego, więc 4× z OSTI nie przenosi się wprost.

### 7.3 Michaelides (EuroSun 2010) — MD nazywa to „sprzeczne", i ma rację

Źródło istnieje. Tezy: straty nocne są **liniowo** zależne od różnicy
temperatur zbiornik–otoczenie, współczynnik korelacji **0,93–0,95**, a przepływ
wsteczny ma wpływ „minimalny albo żaden". Dodatkowo: „energy losses of flat-plate
collectors during night time are mainly due to **conduction** rather than
recirculation".

**Nasze dane nie potwierdzają tej liniowości** (R² ≈ 0). Możliwe przyczyny rozjazdu:
- Michaelides miesza wodę w zbiorniku („thoroughly mixing") — u nas jest
  **stratyfikacja**, a czujnik siedzi w połowie wysokości,
- on liczy **energię** w kJ za całą noc, nie tempo w °C/h,
- u niego nie ma kolektora heat-pipe na dachu w Trzemchy.

Uczciwie: **nasz pomiar nie rozstrzyga sporu w literaturze**, ale jest z nim niesprzeczny —
pokazuje, że raz pętla działa, raz nie, a to właśnie wyjaśnia, dlaczego jedni widzą
liniowość, a inni nie.

### 7.4 Morrison 1986 — POTWIERDZONE pośrednio

Wymieniany przez oba nowsze źródła jako autor badania przepływu wstecznego. Nie
sprawdzane w oryginale.

### 7.5 Czego w literaturze nie ma

Nie znalazłem źródła, które podawałoby **UA zbiornika 1,4 W/K dla 100 l**.
Nasz pomiar mówi 2,9–3,6 W/K. To jest pozycja do sprawdzenia, bo bezpośrednio
wpływa na model.

## 8. Wnioski

1. **Hipoteza użytkownika jest trafna.** Spadki wieczorne to przeważnie wychładzanie,
   nie pobór wody — 26 z 28 nocy bez śladu poboru.
2. **Ale to nie jest wychładzanie „od temperatury powietrza".** Różnica temperatur
   zbiornik–powietrze nie tłumaczy spadków (R² = −0,047). Samo przewodzenie przez
   ścianki daje 2,1× mniej, niż widać.
3. **Brakujące ciepło ucieka przez pętlę kolektora**, i to skokowo: rozkład `UA`
   jest dwumodalny (1–4 W/K albo 14–28 W/K). Pętla albo pracuje, albo stoi.
4. **Model dwuwęzłowy z `MODEL_SOLARNY_TRZEMCHA` §18 jest właściwym podejściem** —
   jedyny, który w ogóle przewiduje spadek temperatury. Ale jego `UA = 1,4 W/K`
   jest **2× za małe** wobec pomiaru.
5. **Spadek nocny 5,6 °C mieści się w normie literaturowej** (2–10 °C, Ong 2015).
   Instalacja nie jest zepsuta.

## 9. Czego ten raport NIE twierdzi

- Nie twierdzi, że 4× z OSTI stosuje się do naszej instalacji (7.1, 7.2).
- Nie rozstrzyga sporu OSTI ↔ Michaelides (7.3).
- Nie ma danych o wietrze, zachmurzeniu nocnym ani nasłonecznieniu w płaszczyźnie
  kolektora dla większości nocy — korelacja z GHI opiera się na **7 nocach**.
- Jeden czujnik w połowie zbiornika nie widzi stratyfikacji ani rozkładu temperatur.

## 10. Co z tego wynika dla aplikacji (do decyzji, nie zrobione)

Nie zmieniono żadnego pliku `.kt`. Pozycje do rozważenia:

| pozycja | dziś | pomiar | skutek |
|---|---|---|---|
| `tankUa` | 1,4 W/K | 2,9–3,6 W/K | model zaniża straty nocne ~2× |
| `reverseFlowKgS` | 0,004 kg/s | niezmierzony | zgadza się z Ong 2015 (mniej niż 0,005 dla płaskiego) |
| karty 7-dniowe | `4,43·GTI − 5,57` | R² modelu nieznane | stała strata 5,57 °C/dobę nie zna temperatury wody |

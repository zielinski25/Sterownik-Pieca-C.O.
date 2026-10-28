# Weryfikacja 0.31.37 i 0.31.38 na telefonie — 2026-10-29

Stan po zrzutach z **00:37** (0.31.37) i **00:47** (0.31.38). Temat **226 domknięty**.

## Potwierdzone na żywo

| Ekran / element | 0.31.36 (23:47) | 0.31.37 (00:37) | 0.31.38 (00:47) |
|---|---|---|---|
| „Zysk/Spadek dziś · zmierzone” | −17,5 °C (bojler pieca) | **0,0 °C · bez zmian** | **0,0 °C · bez zmian** |
| „Pobrania ciepłej wody” | brak | **brak** | **brak** |
| Podpis pod tabelą | brak wzmianki o progu | **„zbiornik słoneczny 100 l na dachu (czujnik kolektora, nie bojler pieca)”** | to samo |
| „Do końca dnia · szacunek” | brak prognozy | brak prognozy | **„słońce nie przebije strat ciepła”** |
| Karta „DO KOŃCA DNIA” | brak prognozy | brak prognozy | **„+0.0°C · słońce nie przebije strat ciepła”** |

Zgodność z `/archiwum` centrali dla 28.10: centrala dT **−0,2 °C**, aplikacja pokazała
−0,3 °C (0.31.35) i −17,5 °C (0.31.36 — błąd), teraz liczy od nowej doby (0,0 °C o 00:37).

## Co poprawił 0.31.37

Powrót do `t_panel` w `recomputeToday()` i `recomputeDraws()`. `t_panel` to woda
w zbiorniku słonecznym na dachu (BUS_3, `czujnik_temp_Panel_Plus`), `t_bojler` to bojler
pieca w kotłowni (BUS_2). Źródło: `ARCHIWUM_SOLARNE_STAN.md` §2–§3.
Szczegóły i sprostowanie błędu 0.31.36 — patrz `WERYFIKACJA-0.31.36.md`.

## Co poprawił 0.31.38

Karta „do końca dnia” po starcie pokazywała „brak prognozy”, bo model potrzebuje
dwóch rzeczy naraz: próbki wody i godzin pogody. Pogoda przychodzi pierwsza,
historia z chmury po niej — model startował bez próbki i wynik zostawał pusty
do następnego odświeżenia pogody (15 min). Teraz po scaleniu historii prognoza jest
liczona ponownie, jeśli wcześniej się nie udała. Komunikat rozdziela też
„brak próbki wody z czujnika” od „brak prognozy”.

## Dwie liczby obok siebie — dlaczego się nie wykluczają

- karta **DZIŚ −4,5 °C** = model dobowy: suma GTI doby × 4,43 − 5,57 straty
  (`dayDeltaC`), liczona od północy do północy,
- karta **DO KOŃCA DNIA +0,0 °C** = symulacja cieplna od bieżącej temperatury wody
  do północy (`SolarThermalModel`).

Przy 29.10 (szczyt 41 W/m²) doba wychodzi −4,5 °C, a od teraz do północy woda nie
przebije strat — oba zdania są prawdziwe i opisują inny horyzont. Podpisy pod sekcją
mówią to wprost („potencjał całej doby, nie od teraz”).

## Co pozostaje nie sprawdzone

**Temat 225 — wykresy.** Brak zrzutu ekranu Wykresy. Do potwierdzenia na 0.31.38:
- zakres **24.10, 12:00–14:00** → podpisana przerwa **„luka 77 min”** (12:26 → 13:43),
- poza luką linie **ciągłe**, bez cichych przerw,
- pasek pompy (`#4ADE80`) nie zasłania linii temperatur.

Dzień 24.10 ma w `/archiwum` pokrycie **93,3%** (1344/1440) — czyli luka jest
potwierdzona po stronie centrali. Zostaje sprawdzić, czy aplikacja ją **opisuje**,
a nie tylko zostawia puste miejsce.

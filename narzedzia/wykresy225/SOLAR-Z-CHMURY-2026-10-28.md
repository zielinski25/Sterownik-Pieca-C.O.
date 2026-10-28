# Temat 226 — panel słoneczny: „brak próbek z dziś” (v0.31.33 → v0.31.35)

Data: 2026-10-28 wieczór. Materiał: 2 zrzuty ekranu z arkusza „Panel słoneczny (solary)"
(20:52:21 i 20:52:25) + eksport bazy Firebase + kod aplikacji v0.31.33.

## 1. Co pokazują ekrany

Arkusz solarny, 28.10 godz. 20:52. Aplikacja „LIVE", panel **17,5 °C**, powietrze **15,0 °C**,
różnica **+2,5 °C**. Pod tym:

| wiersz | wartość na ekranie | poprawnie? |
|---|---|---|
| Panel teraz / powietrze | 17,5 / 15,0 °C | ✅ zgadza się z chmurą (a[5] = 17,5) |
| Różnica panel − powietrze | +2,5 °C | ✅ |
| **Zysk dziś · zmierzone** | **brak próbek z dziś** | ❌ dane SĄ w chmurze |
| Do końca dnia · szacunek | bez słońca do końca dnia | ✅ o 20:52 tak jest |
| Pobrania ciepłej wody | brak | ❌ nie sprawdzono historii |

Karty prognozy 7-dniowej: DZIŚ +0,0 (do 125 W/m²), JUTRO +0,0 (59), Pt 30.10 +3,0 (466),
Sb 31.10 +3,5 (502), Nd 01.11 +2,1 (252), Pn 02.11 +0,0 (219), Wt 03.11 +4,0 (392).

## 2. Dlaczego „brak próbek z dziś” — przyczyna znaleziona w kodzie

`SolarAnalytics.recordSample()` jest wołane **wyłącznie** z `AppModel.recordSolar()`,
a ta funkcja żyje w cyklu pollingu, który startuje w `onResume()` i **jest kasowany
w `onPause()`**. Innymi słowy: zysk „dziś" mierzy się tylko wtedy, gdy telefon ma
aplikację otwartą na wierzchu.

Dodatkowo próbka przepada, gdy brakuje któregokolwiek wejścia — m.in. promieniowania
z pogody (`radEst()` wymaga godziny dokładnie równej początkowi bieżącej godziny
w prognozie Open-Meteo). Efekt: kto zagląda do pieca wieczorem, ten przez cały dzień
nie ma **ani jednej** próbki.

To NIE jest kwestia „danych brak" — chmura ma zbiornik co minutę.

## 3. Co naprawdę wydarzyło się w zbiorniku (z eksportu RTDB)

Indeksy w rekordzie: `a[0]` = zewn., `a[1]` = bojler, `a[5]` = panel (potwierdzone
zgodnością 17,5 °C z ekranem).

| dzień | próbek | zbiornik 100 l | panel max |
|---|---|---|---|
| 26.10 | 1435 | 43,0 → 30,8 °C (**−12,2**) | 19,5 °C |
| 27.10 | 1437 | 30,8 → 36,8 °C (**+6,0**) | **31,3 °C** |
| 28.10 | 1204 | 36,8 → 21,0 °C (**−15,8**) | 20,5 °C |

Czyli: 27.10 kolektor realnie dogrzał wodę o **6 °C** (panel doszedł do 31,3 °C —
przy termosyfonie to normalne, woda krąży grawitacyjnie). 28.10 piec nie grzał,
woda stygła o 15,8 °C. **Aplikacja nie pokazała ani jednej z tych liczb.**

## 4. Poprawka: v0.31.35 „solar z chmury"

Plik: `android-sync/v0.31.35-solar-z-chmury/SterownikCO-Android-v0.31.35-SOLAR.zip`
(10 plików `.kt`, diff, sumy SHA, README). Względem 0.31.34: +116 linii, −5.

1. **`SolarAnalytics.mergeCloudToday()`** — wciąga dzisiejsze próbki z historii chmury
   do archiwum solarnego. Metoda liczenia **bez zmian**: zysk = ostatnia minus pierwsza
   próbka doby, pobranie wody = spadek szybszy niż 0,3 °C/min.
2. **`AppModel.refreshSolarFromCloud()`** — pobiera dobę z RTDB; wywoływane przy
   starcie aplikacji, po pierwszym udanym pollingu i przy otwarciu karty solarnej.
   Limit: jedno pobranie na 5 minut (polling statusu leci co 8 s).
3. **`Sensors.kt`** — opis pod liczbami mówi wprost, że „zmierzone" pochodzi
   z chmury co minutę, także gdy aplikacja była zamknięta.

Wszystko z 0.31.34 (pasek pompy, etykiety luk, `repairGaps`) jest w tej paczce —
**wystarczy wgrać jedną wersję: 0.31.35**.

## 5. Czego świadomie NIE zmieniam

- **Modelu prognozy.** Karty „do X W/m²" pokazują szczyt nasłonecznienia poziomego (GHI),
  a model liczy z irradiancji na płaszczyznę kolektora (GTI). Dlatego kolejność dni nie
  musi odpowiadać szczytom — to nie jest błąd, tylko inne wielkości. Żeby to udowodnić
  liczbami, trzeba porównać z pełną prognozą godzinową, a tej nie mam w eksporcie.
- **Progu 6 minut** i reszty rysowania wykresów.
- **Firmware'u.** Nic tu nie wymaga zmiany w centrali.

## 6. Co będzie widać po wgraniu (wzorzec do sprawdzenia)

Na 28.10 (dzień, w którym robiono zrzuty) karta powinna pokazać
**„Spadek dziś · zmierzone −15,8 °C"**. Jeśli tego dnia woda nie była pobierana,
„Pobrania ciepłej wody" zostanie „brak" — i to będzie prawda, nie brak danych.

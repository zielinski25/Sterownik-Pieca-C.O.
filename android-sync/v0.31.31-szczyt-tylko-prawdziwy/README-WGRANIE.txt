════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.30  →  v0.31.31
 Godzina szczytu tylko wtedy, gdy szczyt naprawdę jest
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.30. Podmienia 3 pliki. Zero nowych plików.
ZERO ręcznych zmian w kodzie.


| | |
|---|---|
| ZIP          | SterownikCO-Android-v0.31.31-SZCZYT.zip |
| wersja       | versionCode 3037 / "v0.31.31" |
| pliki .kt    | 3 (SolarAnalytics.kt, Weather.kt, Sensors.kt) |


────────────────────────────────────────────────────────────────────────────
 0. HASHE
────────────────────────────────────────────────────────────────────────────

  SolarAnalytics.kt   304 linie   15 498 B   (core\)
    77832507553BE910EE09186615900AEAE84DBB4364777F98B32367F3E86A4542

  Weather.kt          927 linii   51 460 B   (ui\screens\)
    F013DDD4FC0729A20545C89E6669FD96F92CDCB8AEF259F6BE97BA7364A62954

  Sensors.kt          408 linii   21 382 B   (ui\screens\sheets\)
    4226DEBA1F19F418D980A93287F14EDF4FDA596D2A078A1A03016DD9DC03AC21


────────────────────────────────────────────────────────────────────────────
 1. CO BYŁO NIE TAK
────────────────────────────────────────────────────────────────────────────

Model liczy dobową symulację i zapamiętuje godzinę, o której woda była
najcieplejsza. Ale zapisuje ją TYLKO wtedy, gdy woda faktycznie się ogrzała.
Jeśli przez całą dobę tylko stygła — zostaje zero.

A zero, zamienione na godzinę, daje 00:00 od północy... czyli w praktyce
bieżącą godzinę, w której model liczył. Karta pokazywała więc na przykład:

    +0,0 °C
    szczyt 08:00 · 0,0 kWh        ← godzina wyssana z palca

KIEDY TO WYCHODZI: mroźny poranek. Słońce świeci słabo, woda po nocy zimna,
straty ciepła większe niż to, co kolektor zbierze. Model poprawnie liczy
wychładzanie — ale etykieta kłamie.

To ten sam problem, który naprawiliśmy wieczorem w v0.31.30, tylko wewnątrz
dnia. Wtedy „+0,0 °C” znaczyło „słońce zaszło”. Teraz może znaczyć
„słońce świeci, ale nie przebije strat”.


────────────────────────────────────────────────────────────────────────────
 2. CO SIĘ ZMIENIA
────────────────────────────────────────────────────────────────────────────

A) NOWA FLAGA W MODELU: forecastPeakReal

   SolarAnalytics.kt dostaje pole mówiące wprost, czy szczyt jest prawdziwy:

       forecastPeakReal = res.deltaPeakC > PEAK_EPS_C

   PEAK_EPS_C = 0,05 °C — próg leży pod rozdzielczością wyświetlania (0,1 °C),
   więc nie pokażemy „+0,0 °C · szczyt 08:00”.

   Etykieta z godziną jest ustawiana TYLKO przy prawdziwym szczycie.
   W przeciwnym razie zostaje pusta — nikt jej nie użyje przez przypadek.


B) KARTA „DO KOŃCA DNIA” MÓWI WPROST, DLACZEGO JEST ZERO

   Zamiast zmyślonej godziny:

       +0,0 °C
       słońce nie przebije strat ciepła


C) TO SAMO W ARKUSZU CZUJNIKI

   Wiersz „Do końca dnia · szacunek” dostaje ten sam warunek.
   Jedno źródło, nie dwa różne rachunki.


────────────────────────────────────────────────────────────────────────────
 3. WSZYSTKIE PRZYPADKI — CO ZOBACZYSZ
────────────────────────────────────────────────────────────────────────────

  SYTUACJA                              KARTA                    CZUJNIKI
  ─────────────────────────────────────────────────────────────────────────
  po zachodzie słońca                   —                        bez słońca…
                                        bez słońca do końca…     do końca dnia

  dzień słoneczny, woda się grzeje      +2,3 °C                  +2,3°C · szczyt
                                        szczyt 14:06 · 2,9 kWh   14:06 · 2,9 kWh

  mroźny poranek, samo wychładzanie     +0,0 °C                  słońce nie przebije
                                        słońce nie przebije…     strat ciepła

  brak danych o pogodzie                —                        brak prognozy
                                        brak prognozy


────────────────────────────────────────────────────────────────────────────
 4. CO ZROBIĆ — 4 KROKI
────────────────────────────────────────────────────────────────────────────

KROK 1. Podmień trzy pliki

  SolarAnalytics.kt  →  app\src\main\java\com\sterownikco\pro\core\
  Weather.kt         →  app\src\main\java\com\sterownikco\pro\ui\screens\
  Sensors.kt         →  app\src\main\java\com\sterownikco\pro\ui\screens\sheets\

  TRZY RÓŻNE FOLDERY. Sensors.kt leży o poziom niżej, w `sheets`.

  Pytanie „plik istnieje, zamienić?” → TAK.


KROK 2. Podbij wersję

  Plik: app\build.gradle.kts

  Znajdź:
      versionCode = 3036
      versionName = "v0.31.30"

  Zamień na:
      versionCode = 3037
      versionName = "v0.31.31"


KROK 3. Zbuduj

  cd C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  .\gradlew assembleDebug

  Oczekiwany koniec:
      BUILD SUCCESSFUL


KROK 4. Wgraj

  app\build\outputs\apk\debug\app-debug.apk

  Po instalacji w „Informacje o aplikacji” ma pisać: v0.31.31-debug


────────────────────────────────────────────────────────────────────────────
 5. JAK SPRAWDZIĆ, ŻE DZIAŁA
────────────────────────────────────────────────────────────────────────────

Najprościej wieczorem, po zachodzie — karta „DO KOŃCA DNIA” ma pokazać:

    —
    bez słońca do końca dnia

To znaczy, że warunek działa. Rano, przy słabym słońcu, podpis zmieni się na
„słońce nie przebije strat ciepła” — ale tylko wtedy, gdy model faktycznie
policzy wychładzanie.

Jeśli w słoneczny dzień podpis brzmi „szczyt 14:06 · 2,9 kWh” — wszystko
w porządku, szczyt jest prawdziwy.


────────────────────────────────────────────────────────────────────────────
 6. JEŚLI SIĘ NIE KOMPILUJE
────────────────────────────────────────────────────────────────────────────

  Unresolved reference: forecastPeakReal
      → SolarAnalytics.kt nie został podmieniony. Musi być w core\.

  Unresolved reference: PEAK_EPS_C
      → ten sam plik. Stała siedzi w companion object klasy SolarAnalytics.

  Unresolved reference: solarSunLeft
      → Weather.kt z v0.31.30 nie został podmieniony.

  Cokolwiek innego — wklej komunikat, sprawdzę.


────────────────────────────────────────────────────────────────────────────
 7. CZEGO TO NIE ROBI
────────────────────────────────────────────────────────────────────────────

  - Nie zmienia modelu cieplnego ani żadnego rachunku. Parametry bez zmian.
  - Nie zmienia wyglądu karty poza jednym podpisem.
  - Nie zgaduje pogody — czyta to, co model policzył, i mówi wprost,
    gdy policzył samo wychładzanie.
════════════════════════════════════════════════════════════════════════════

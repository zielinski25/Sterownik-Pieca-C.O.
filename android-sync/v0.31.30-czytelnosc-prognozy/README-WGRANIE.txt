════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.29  →  v0.31.30
 Czytelność prognozy: koniec dwóch „DZIŚ”, godzina szczytu, szczyt z kolektora
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.29. Podmienia 2 pliki. Zero nowych plików.
ZERO ręcznych zmian w kodzie.

Ta paczka NIE zmienia modelu ani rachunków. Model jest już poprawny —
v0.31.29 pokazał to na ekranie (DZIŚ +16,0 °C liczone z GTI, nie z GHI).
Ta wersja poprawia tylko to, jak liczby są podpisane i co pokazują.


| | |
|---|---|
| ZIP          | SterownikCO-Android-v0.31.30-CZYTELNE.zip |
| wersja       | versionCode 3036 / "v0.31.30" |
| pliki .kt    | 2 (Weather.kt, Sensors.kt) |


────────────────────────────────────────────────────────────────────────────
 0. HASHE — PO CZYM POZNASZ, ŻE MASZ DOBRY PLIK
────────────────────────────────────────────────────────────────────────────

    Get-FileHash "ŚCIEŻKA\Weather.kt" -Algorithm SHA256

  Weather.kt    923 linie    51 170 B   (app\src\main\java\com\sterownikco\pro\ui\screens\)
    386B367A6EA5F413F6E2B9AEFBA6887D088D4DF411606E1FFC9E1DB3AC97246E

  Sensors.kt    406 linii    21 204 B   (app\src\main\java\com\sterownikco\pro\ui\screens\sheets\)
    B5DEA99128C693C3F13AD8B558BE9F986E87324E70EA1495D918EF19E07E8BC1

  Podstawa (Twoje pliki v0.31.28) — nimi się to liczyło:
    Weather.kt    81E6C58BF52064FD6A28F6131AFC2617F0FC160C527344CB1E2E396141465AD1
    Sensors.kt    770A4D65F5E6DAD7FBFF59C14C00288BF12FF7307550319F41CA8ED33FBA9EC6


────────────────────────────────────────────────────────────────────────────
 1. CO BYŁO NIE TAK NA EKRANIE
────────────────────────────────────────────────────────────────────────────

Zrzut z 23.10.2026, godz. 18:16. Na jednym ekranie stały obok siebie:

    🔮 PROGNOZA DZIŚ      +0,0 °C
    📅 DZIŚ (7 dni)      +16,0 °C

Dwie liczby, obie „dziś”, różnica 16 stopni. Wyglądało jak błąd.

Nie było błędu. Pierwsza mówiła „od teraz do północy” — a słońce zaszło
o 17:25, więc model nie miał czego grzać i uczciwie wyszło zero.
Druga mówiła „cały dzień”.

Problem był w nazwie, nie w rachunku.

Do tego karta 7-dniowa pokazywała „do 436 W/m²” — a to szczyt z płaszczyzny
POZIOMEJ. Na kolektor pod kątem 45° padało wtedy 778 W/m². Prawie dwa razy
więcej niż pokazane.


────────────────────────────────────────────────────────────────────────────
 2. CO SIĘ ZMIENIA
────────────────────────────────────────────────────────────────────────────

A) KARTA „PROGNOZA DZIŚ”  →  „DO KOŃCA DNIA”

   Nazwa mówi teraz, że to horyzont od teraz do północy — nie cała doba.
   Nie myli się już z „DZIŚ” z siatki 7-dniowej.

   Po zachodzie słońca pokazuje „—” zamiast „+0,0 °C”, a pod spodem:
       bez słońca do końca dnia


B) POD LICZBĄ DOCHODZI SZCZYT I ENERGIA

   Model liczył to od v0.31.29, ale nigdzie tego nie pokazywał. Teraz widać:

       +2,3 °C
       szczyt 14:06 · 2,9 kWh

   „szczyt” to godzina, o której woda będzie najcieplejsza.
   „kWh” to energia dostarczona przez kolektor — szacunek, nie pomiar.


C) KARTA 7-DNIOWA: SZCZYT Z KOLEKTORA, NIE Z DACHU

   „do 436 W/m²”  →  „do 778 W/m²”

   Program bierze teraz GTI (płaszczyzna kolektora pod kątem 45°).
   GHI zostaje jako zapas, gdy API nie zwróci GTI.
   Na samym końcu — szacunek z zachmurzenia, jak dotąd.


D) TO SAMO W ARKUSZU CZUJNIKI

   Wiersz „Prognoza na dziś · szacunek” zmienia się na
   „Do końca dnia · szacunek” i pokazuje pełną wartość:

       +2,3°C · szczyt 14:06 · 2,9 kWh

   Arkusz czyta ten sam warunek co karta na ekranie Pogoda — jedno źródło,
   nie dwa różne rachunki.


────────────────────────────────────────────────────────────────────────────
 3. ZANIM ZACZNIESZ
────────────────────────────────────────────────────────────────────────────

Kopia zapasowa projektu:

  C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  C:\Users\Rafcio\Desktop\SterownikCO-0.31.23-KOPIA


────────────────────────────────────────────────────────────────────────────
 4. CO ZROBIĆ — 4 KROKI
────────────────────────────────────────────────────────────────────────────

KROK 1. Podmień dwa pliki

  Weather.kt  →  app\src\main\java\com\sterownikco\pro\ui\screens\
  Sensors.kt  →  app\src\main\java\com\sterownikco\pro\ui\screens\sheets\

  Uwaga na foldery — pliki NIE idą do tego samego katalogu.
  Sensors.kt leży o poziom niżej, w `sheets`.

  Pytanie „plik już istnieje, zamienić?” → TAK.


KROK 2. Podbij wersję

  Plik: app\build.gradle.kts

  Znajdź:
      versionCode = 3035
      versionName = "v0.31.29"

  Zamień na:
      versionCode = 3036
      versionName = "v0.31.30"


KROK 3. Zbuduj

  cd C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  .\gradlew assembleDebug

  Oczekiwany koniec:
      BUILD SUCCESSFUL

  Plik:
      app\build\outputs\apk\debug\app-debug.apk


KROK 4. Wgraj na telefon

  Po instalacji sprawdź w „Informacje o aplikacji”:
      wersja v0.31.30-debug


────────────────────────────────────────────────────────────────────────────
 5. CO ZOBACZYSZ PO WGRANIU
────────────────────────────────────────────────────────────────────────────

Zaraz po wgraniu (wieczór, po zachodzie):

  🔮 DO KOŃCA DNIA
      —
      bez słońca do końca dnia

  📅 DZIŚ              +16,0 °C      do 778 W/m²
     JUTRO             +9,6 °C       do 426 W/m²

Następnego dnia rano:

  🔮 DO KOŃCA DNIA
      +2,3 °C
      szczyt 14:06 · 2,9 kWh

JAK SPRAWDZIĆ, ŻE DZIAŁA:

  „do XXX W/m²” na karcie DZIŚ ma być WYŻSZE niż dotąd w słoneczne dni.
  Dla 23.10 było 436 W/m², teraz ma być 778 W/m².

  To nie zmiana modelu — to pokazanie właściwej płaszczyzny.


────────────────────────────────────────────────────────────────────────────
 6. JEŚLI SIĘ NIE KOMPILUJE
────────────────────────────────────────────────────────────────────────────

  Unresolved reference: solarSunLeft
      → Weather.kt nie został podmieniony albo Sensors.kt trafił do złego
        folderu. Sensors.kt MUSI być w ...\ui\screens\sheets\

  Unresolved reference: forecastEnergyKwh / forecastPeakLabel
      → na telefonie nie ma v0.31.29 w kodzie. Ta paczka zakłada, że
        SolarAnalytics.kt z v0.31.29 jest już w projekcie.

  Unresolved reference: tiltedIrradiance
      → brak Weather.kt z v0.31.29 (core, nie mylić z tym z ui\screens).

  Cokolwiek innego — wklej komunikat, sprawdzę.


────────────────────────────────────────────────────────────────────────────
 7. CZEGO TO NIE ROBI
────────────────────────────────────────────────────────────────────────────

  - Nie zmienia modelu ani rachunków. Ten sam wzór co w v0.31.29.
  - Nie zgaduje, czy woda będzie cieplejsza wieczorem — jeśli słońce zaszło,
    pokazuje „—” i mówi wprost, dlaczego.
  - Nadal nie liczy poboru ciepłej wody ani strat na rurach.
  - Model sprawdzony na CZTERECH dniach. To mało.
════════════════════════════════════════════════════════════════════════════

════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.28  →  v0.31.29
 Model cieplny kolektora zamiast mnożenia kWh/m² × 4,2
════════════════════════════════════════════════════════════════════════════

POPRAWKA 2026-10-23. Pierwsza wersja tej paczki miała błąd: w funkcji
`estKwh()` mnożnik 200 zamiast 100. Podwajało to szacunek kWh i cofało
poprawkę z wersji 0.31.26 (zbiornik ma 100 litrów, nie 200 kg).

Ta wersja jest zbudowana z PLIKÓW Z TWOJEGO TELEFONU (v0.31.28,
versionCode 3034), a nie z odtworzonej kopii. Sprawdzone hashem.

Nakładka na drzewo v0.31.28. Podmienia 2 pliki, dodaje 1 nowy.
ZERO ręcznych zmian w kodzie.


| | |
|---|---|
| ZIP          | SterownikCO-Android-v0.31.29-MODEL.zip |
| wersja       | versionCode 3035 / "v0.31.29" |
| pliki w ZIP  | 3 pliki .kt + ten README |


────────────────────────────────────────────────────────────────────────────
 0. HASHE — PO CZYM POZNASZ, ŻE MASZ DOBRY PLIK
────────────────────────────────────────────────────────────────────────────

Jeśli chcesz się upewnić, że skopiowałeś właściwy plik, policz SHA256:

    Get-FileHash "ŚCIEŻKA\SolarAnalytics.kt" -Algorithm SHA256

  SolarAnalytics.kt      288 linii    14 424 B
    1FBD82686DD8FED69D2F30D14FFAF4F30E45BF47BD3C49DF3C473A716407E630

  Weather.kt             202 linie    10 594 B
    493009396FEA4FF889D52253E7C5C24DB615117F02F714E12D3A155F50EFFD7E

  SolarThermalModel.kt   224 linie    10 397 B
    DBC8A0DBCFE7688B9510CE69FB3218BB0D558FD2E622B3EB48447EA87F4AAA38


────────────────────────────────────────────────────────────────────────────
 1. CO TO ZMIENIA
────────────────────────────────────────────────────────────────────────────

„ZYSK DZIŚ +21,5 °C” — to była SUMA samych wzrostów temperatury.
Każdy spadek był pomijany. Przy termosyfonie woda krąży pulsami, więc
czujnik drga o ±0,1 °C, a każde drgnięcie w górę doliczało się od nowa.

  Zmierzone 2026-10-23: woda 29,2 → 44,8 °C.
  Fizyczne maksimum: +15,6 °C.  Aplikacja pokazywała +21,5 °C.

  TERAZ: pokazuje różnicę netto. Nie może przekroczyć fizyki.

„PROGNOZA DZIŚ +11,2 °C” — to było 2,65 kWh/m² × 4,2, na słońcu
z PŁASZCZYZNY POZIOMEJ. Kolektor stoi pod kątem ~45°, więc dostaje GTI,
które ma się do GHI jak 0,88 (pochmurno) do 1,83 (bezchmurnie).
Do tego nie było członu strat, więc prognoza nie umiała pokazać spadku.

  TERAZ: model fizyczny godzinowy, od bieżącej temperatury wody.

  Błąd na 4 dniach pomiarów:   stary model 3,4 – 6,6 °C
                               nowy model   0,6 – 1,5 °C

  I po raz pierwszy prognoza potrafi pokazać, że woda się WYCHŁADZA.


────────────────────────────────────────────────────────────────────────────
 2. CO JEST W PACZCE
────────────────────────────────────────────────────────────────────────────

  Wszystkie trzy idą do tego samego folderu:

    app\src\main\java\com\sterownikco\pro\core\

  SolarThermalModel.kt    NOWY plik — nie istnieje, więc nic nie nadpisujesz
  SolarAnalytics.kt       NADPISUJE istniejący
  Weather.kt              NADPISUJE istniejący — PEŁNY plik, gotowy

  Weather.kt dostaje pole `tiltedIrradiance` i zapytanie o GTI pod kątem 45°.
  To jedyna zmiana w tym pliku — 19 linii, wszystkie dotyczą nasłonecznienia
  w płaszczyźnie kolektora. Reszta pliku jest nietknięta.

  NIC NIE MUSISZ JUŻ POPRAWIAC RĘCZNIE.


────────────────────────────────────────────────────────────────────────────
 3. ZANIM ZACZNIESZ
────────────────────────────────────────────────────────────────────────────

Skopiuj cały folder projektu gdzieś obok, np.:

  C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  C:\Users\Rafcio\Desktop\SterownikCO-0.31.23-KOPIA

Jeśli coś pójdzie nie tak, wracasz do kopii. Bez tego ani rusz.


────────────────────────────────────────────────────────────────────────────
 4. CO ZROBIĆ — 4 KROKI
────────────────────────────────────────────────────────────────────────────

KROK 1. Skopiuj trzy pliki

  Wszystkie trzy z paczki wklej do:

    C:\Users\Rafcio\Desktop\SterownikCO-0.31.23\app\src\main\java\com\sterownikco\pro\core\

  Pytanie „plik już istnieje, zamienić?” → TAK.


KROK 2. Podbij wersję

  Plik: app\build.gradle.kts

  Znajdź:
      versionCode = 3034
      versionName = "v0.31.28"

  Zamień na:
      versionCode = 3035
      versionName = "v0.31.29"


KROK 3. Zbuduj

  cd C:\Users\Rafcio\Desktop\SterownikCO-0.31.23
  .\gradlew assembleDebug

  Oczekiwany koniec:
      BUILD SUCCESSFUL

  Plik:
      app\build\outputs\apk\debug\app-debug.apk


KROK 4. Wgraj na telefon

  Ten sam plik APK co zawsze. Po instalacji sprawdź w „Informacje
  o aplikacji”, że pisze: wersja v0.31.29-debug.


────────────────────────────────────────────────────────────────────────────
 5. CO ZOBACZYSZ PO WGRANIU
────────────────────────────────────────────────────────────────────────────

Karta „BILANS & ZYSK KOLEKTORA SŁONECZNEGO”:

  ZYSK DZIŚ        — inna liczba. NIŻSZA. To jest dobrze.
                     Wcześniej +21,5 °C, teraz około +15 °C.

  PROGNOZA DZIŚ    — inna liczba, zależna od temperatury wody.
                     Przy zimnej wodzie wyższa, przy ciepłej niższa.

Karty 7-dniowe — też się zmienią, bo korzystają z tego samego wzoru.

JAK SPRAWDZIĆ, ŻE DZIAŁA:

  Porównaj „ZYSK DZIŚ” z różnicą temperatur:
      (ostatni odczyt panelu) − (pierwszy odczyt z dziś)

  Powinno się zgadzać co do 0,1 °C.
  Jeśli „ZYSK DZIŚ” jest WYŻSZY od tej różnicy — coś poszło nie tak.


────────────────────────────────────────────────────────────────────────────
 6. JEŚLI SIĘ NIE KOMPILUJE
────────────────────────────────────────────────────────────────────────────

  Unresolved reference: tiltedIrradiance
      → Weather.kt nie został podmieniony. Wróć do KROKU 1.
        Sprawdź hash — ma być 49300939…

  Unresolved reference: SolarThermalModel
      → SolarThermalModel.kt nie trafił do folderu core/.
        Musi być w .../com/sterownikco/pro/core/.

  Unresolved reference: simulate
      → zły plik SolarThermalModel.kt. Funkcja nazywa się `simulate`.

  Cokolwiek innego — wklej komunikat, sprawdzę.


────────────────────────────────────────────────────────────────────────────
 7. CZEGO TO JESZCZE NIE ROBI
────────────────────────────────────────────────────────────────────────────

  - Pobór ciepłej wody nie jest wliczany do prognozy.
  - Straty na rurach nie są liczone (mogą zjadać 0,2–0,5 kWh/dobę).
  - Model sprawdzony na CZTERECH dniach. To mało.
    Po 2–3 tygodniach trzeba dopasować parametry na większej próbce.

  Model jest wyraźnie lepszy od obecnego, ale nie jest jeszcze
  wiarygodny co do dziesiątej części stopnia.

  Szczegóły i źródła: MODEL_SOLARNY_TRZEMCHA_2026-10-23.md w repozytorium.


────────────────────────────────────────────────────────────────────────────
 8. ŹRÓDŁO LICZB
────────────────────────────────────────────────────────────────────────────

  Kolektor   Apricus ETC-30 (10 rur 58/1800, heat pipe)
             η₀ = 0,714   a₁ = 1,243 W/(m²·°C)   a₂ = 0,009
             apertura 0,95 m²

  Zbiornik   100 l  →  418,6 kJ/°C
             UA = 1,4 W/°C (literatura: 1,07–1,46)

  Lokalizacja  51.066389 N, 21.509167 E  (Trzemcha Górna)

  Wszystkie wartości katalogowe. NIC nie jest dopasowane do Twojego dachu.
════════════════════════════════════════════════════════════════════════════

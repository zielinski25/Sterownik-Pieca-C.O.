════════════════════════════════════════════════════════════════════════════
 SterownikCO Android  v0.31.28  →  v0.31.29
 Model cieplny kolektora zamiast mnożenia kWh/m² × 4,2
════════════════════════════════════════════════════════════════════════════

Nakładka na drzewo v0.31.28. Nadpisuje 1 plik, dodaje 1 nowy,
wymaga 3 drobnych zmian w Weather.kt.

| | |
|---|---|
| ZIP          | SterownikCO-Android-v0.31.29-MODEL.zip |
| rozmiar      | 13 801 B (12 wpisów) |
| sha256       | 88428ad93569e2d745cef953385e4bade08698a2e904ad82caae182e7aaf6b1c |
| wersja       | versionCode 3035 / "v0.31.29" |


────────────────────────────────────────────────────────────────────────────
 1. CO TO ZMIENIA
────────────────────────────────────────────────────────────────────────────

„ZYSK DZIŚ +21,5 °C" — to była SUMA samych wzrostów temperatury.
Każdy spadek był pomijany. Przy termosyfonie woda krąży pulsami, więc
czujnik drga o ±0,1 °C, a każde drgnięcie w górę doliczało się od nowa.

  Zmierzone 2026-10-23: woda 29,2 → 44,8 °C.
  Fizyczne maksimum: +15,6 °C.  Aplikacja pokazywała +21,5 °C.

  TERAZ: pokazuje różnicę netto. Nie może przekroczyć fizyki.

„PROGNOZA DZIŚ +11,2 °C" — to było 2,65 kWh/m² × 4,2, na słońcu
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

  SolarThermalModel.kt    NOWY plik — sam model, nic nie nadpisuje
  SolarAnalytics.kt       NADPISUJE istniejący
  Weather-GTI-INSTRUKCJA.txt   3 zmiany, które musisz zrobić ręcznie
  ZMIANY-0.31.28-do-0.31.29.diff   to samo co wyżej, jako łatka

  SterownikCO-Android-v0.31.29-MODEL.zip   wszystkie pliki razem


────────────────────────────────────────────────────────────────────────────
 3. ZANIM ZACZNIESZ
────────────────────────────────────────────────────────────────────────────

Skopiuj cały folder projektu gdzieś obok, np.:

  SterownikCO-0.31.28  →  SterownikCO-0.31.28-KOPIA

Jeśli coś pójdzie nie tak, wracasz do kopii. Bez tego ani rusz.


────────────────────────────────────────────────────────────────────────────
 4. CO ZROBIĆ — 5 KROKÓW
────────────────────────────────────────────────────────────────────────────

KROK 1. Nowy plik

  SolarThermalModel.kt  →  app/src/main/java/com/sterownikco/pro/core/

  Plik nie istnieje, więc nic nie nadpisujesz. Po prostu wklej.


KROK 2. Nadpisz

  SolarAnalytics.kt  →  app/src/main/java/com/sterownikco/pro/core/

  (ten sam folder, ten sam plik — podmień zawartość)


KROK 3. Popraw Weather.kt

  Otwórz Weather-GTI-INSTRUKCJA.txt i zrób 3 zmiany.
  BEZ TEGO SIĘ NIE SKOMPILUJE.


KROK 4. Podbij wersję

  Plik: app/build.gradle.kts

  Znajdź:
      versionCode = 3034
      versionName = "v0.31.28"

  Zamień na:
      versionCode = 3035
      versionName = "v0.31.29"


KROK 5. Zbuduj

  cd C:\Users\Rafcio\Desktop\SterownikCO-0.31.28-KOPIA
  .\gradlew assembleDebug

  Oczekiwany koniec:
      BUILD SUCCESSFUL

  Plik:
      app\build\outputs\apk\debug\app-debug.apk


────────────────────────────────────────────────────────────────────────────
 5. CO ZOBACZYSZ PO WGRANIU
────────────────────────────────────────────────────────────────────────────

Karta „BILANS & ZYSK KOLEKTORA SŁONECZNEGO":

  ZYSK DZIŚ        — inna liczba. NIŻSZA. To jest dobrze.
                     Wcześniej +21,5 °C, teraz około +15 °C.

  PROGNOZA DZIŚ    — inna liczba, zależna od temperatury wody.
                     Przy zimnej wodzie wyższa, przy ciepłej niższa.

Karty 7-dniowe — też się zmienią, bo korzystają z tego samego wzoru.

JAK SPRAWDZIĆ, ŻE DZIAŁA:

  Porównaj „ZYSK DZIŚ" z różnicą temperatur:
      (ostatni odczyt panelu) − (pierwszy odczyt z dziś)

  Powinno się zgadzać co do 0,1 °C.
  Jeśli „ZYSK DZIŚ" jest WYŻSZY od tej różnicy — coś poszło nie tak.


────────────────────────────────────────────────────────────────────────────
 6. JEŚLI SIĘ NIE KOMPILUJE
────────────────────────────────────────────────────────────────────────────

  Unresolved reference: tiltedIrradiance
      → KROK 3 nie zrobiony. Wróć do instrukcji Weather.kt.

  Unresolved reference: SolarThermalModel
      → KROK 1 nie zrobiony, albo plik wylądował w złym folderze.
        Musi być w .../com/sterownikco/pro/core/.

  Unresolved reference: simulator / simulate
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

STEROWNIK CO — APLIKACJA ANDROID v0.31.48 (versionCode 31048)
============================================================

CO SIĘ ZMIENIŁO WOBEC 0.31.47 — JEDNA RZECZ
-------------------------------------------
Karty „PROGNOZA POTENCJAŁU SOLARNEGO NA 7 DNI" liczą teraz modelem
FIZYCZNYM, dzień po dniu, a nie wzorem liniowym.

DLACZEGO — dowód z ekranu z 29.10.2026, godz. 11:33:
    DO KOŃCA DNIA      +2.5 °C     (model fizyczny)
    DZIŚ (karta 7 dni) −2.4 °C     (wzór liniowy)
    zmierzone do 11:23 +0.5 °C     (czujnik)

Dwie liczby na jednym ekranie przeczyły sobie o 5 °C w przeciwnych
kierunkach. Woda realnie się GRZAŁA, a karta twierdziła, że dzień
jest stracony.

Przyczyna: wzór `4,43 × GTI − 5,57` ma wpisaną na sztywno dobową
stratę 5,57 °C, niezależnie od temperatury wody i powietrza. Dnia
29.10 woda miała 17,5 °C, powietrze 13,0 °C — różnica 4,5 K, czyli
realna strata około 1,3 °C, a nie 5,57 °C.

CO JEST TERAZ
-------------
DZIŚ = zmierzone do teraz + model na resztę doby.
       Czyli dokładnie to samo, co karta „DO KOŃCA DNIA", plus to,
       co już się zmierzyło. Z definicji nie mogą sobie przeczyć.

Dni 2–7 = model fizyczny liczony dzień po dniu. Każdy dzień startuje
          z temperatury, w jakiej skończył się poprzedni — tak jak
          symulacja ciągła z MODEL_SOLARNY_TRZEMCHA §18.5.

Podstawa: MODEL_SOLARNY_TRZEMCHA_2026-10-23.md §14.3 („co podmienić
w liczeniu zysku") i §18.5 (RMS 2,63 °C dla modelu dwuwęzłowego).

CZEGO NIE ZMIENIONO
-------------------
- Wygląd: te same karty, ten sam układ, te same napisy, żadnych
  nowych plakietek, ikonek ani liczników.
- Podpisy pod nagłówkiem — zostały bez zmian i nadal są prawdziwe.
- „DO KOŃCA DNIA", „ZYSK DZIŚ", „POBRANIA DZIŚ" — bez zmian.
- Model fizyczny (SolarThermalModel.kt) — ANI JEDEN PARAMETR.
  Nie ruszano tankUa, reverseFlowKgS, loopUaWk, apertury.
- Wszystko z 0.31.33–0.31.47: szybkość wykresów, pamięć historii,
  rysowanie, bufor offline, SERWO w %.
- Centrala i firmware — bez zmian.

PLIKI ZMIENIONE (3)
-------------------
1. android/app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
   + pole forecastEndDeltaC (zmiana do końca doby ze znakiem)
   + funkcja dailyForecast7() — prognoza 7 dni modelem fizycznym

2. android/app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
   + SolarDayCard() przyjmuje wyliczoną wartość (deltaC)
   + obie siatki 7-dniowe (strona Pogoda i arkusz panelu) liczą ją
     raz, wspólnie

3. android/app/build.gradle.kts
   versionCode 31047 → 31048, versionName 0.31.47 → 0.31.48

JAK ZBUDOWAĆ
-----------
Potrzebujesz JDK 17–21. Skrypt sam je znajdzie i zapisze ścieżkę
do lokalnego pliku konfiguracyjnego.

1) Rozpakuj paczkę.
2) Wejdź do rozpakowanego folderu (tam, gdzie leży zbuduj.ps1).
3) W PowerShell:

       powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

4) Na końcu musisz zobaczyć BUILD SUCCESSFUL oraz ścieżkę do pliku
   app\build\outputs\apk\debug\app-debug.apk

JEŚLI SKRYPT SIĘ NIE UDA
------------------------
Otwórz folder w Android Studio i: Build → Build Bundle(s)/APK(s) → Build APK(s).

Przed budowaniem sprawdź, czy Android Studio ma ustawione JDK 17–21:
File → Settings → Build, Execution, Deployment → Build Tools → Gradle
→ Gradle JDK.

JAK SPRAWDZIĆ, ŻE DZIAŁA
------------------------
1. Zainstaluj APK na telefonie.
2. Ustawienia → Aplikacje → Sterownik CO → wersja musi być 0.31.48.
3. Wejdź w zakładkę Pogoda, sekcja „BILANS & ZYSK KOLEKTORA".
4. Karta DZIŚ i karta DO KOŃCA DNIA muszą teraz wskazywać TEN SAM
   kierunek. Wcześniej jedna mówiła plus, druga minus.
5. Karty na kolejne dni nie powinny już pokazywać minusa w dzień,
   w którym woda ma 17 °C przy powietrzu 13 °C.

UCZCIWIE — CZEGO MODEL NADAL NIE UMIE
-------------------------------------
- Godzina szczytu jest około 2 h za późna (skutek traktowania
  zbiornika jako jednej bryły — patrz §18.6). Nie naprawione.
- Model nie zna poborów ciepłej wody.
- Parametry sprawdzone na 4–5 dobach. To nadal SZACUNEK, nie pomiar
  — i tak jest opisane na ekranie.

POWRÓT DO POPRZEDNIEJ WERSJI
----------------------------
Wgraj APK 0.31.47. Aplikacja nie zmienia formatu zapisu danych,
więc powrót jest bezpieczny — ustawienia i historia zostają.

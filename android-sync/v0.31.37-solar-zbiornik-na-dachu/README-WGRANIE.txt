Sterownik CO — wersja 0.31.37 (Android)
======================================
POPRAWKA MOJEJ POMYŁKI z 0.31.36.

CO ZEPSUŁEM W 0.31.36
---------------------
W 0.31.36 przełączyłem „Zysk / Spadek dziś” i „Pobrania ciepłej wody”
z kanału `tPanel` na `tBojler`. Było to błędne, bo:

  - `tPanel` (czujnik_temp_Panel_Plus, BUS_3, TELEMETRY_CH_PANEL)
    to WODA w zbiorniku słonecznym 100 l NA DACHU,
  - `tBojler` (czujnik_temp_Bojlera, BUS_2) to BOJLER PIECA w kotłowni.

Źródło: ARCHIWUM_SOLARNE_STAN.md §2 (tabela kanałów) i §3 (instalacja:
kolektor 10 rur, zbiornik 100 l na dachu, termosyfon, czujnik w wodzie
w połowie wysokości zbiornika).

Skutek pomyłki: 28.10 aplikacja pokazywała −17,5 °C — to był spadek
temperatury bojlera pieca w kotłowni. Zbiornik słoneczny zmienił się
tego dnia o −0,2 °C, i to jest liczba prawdziwa dla solarów.

CO JEST W 0.31.37
-----------------
1. „Zysk / Spadek dziś” liczone znowu z `tPanel` — czyli z wody w zbiorniku
   słonecznym na dachu. Metoda bez zmian: ostatnia próbka doby − pierwsza.
2. „Pobrania ciepłej wody” też z `tPanel` (wcześniej patrzyły na bojler pieca).
   Reguła: spadek ≥ 0,5 °C od maksimum z 5 minut, utrzymany przez następne
   5 minut; epizody bliższe niż 20 min scalane; spadek > 12 °C ODRZUCANY
   jako usterka sondy (100 l nie traci 25 °C od kranu).
3. Podpis pod tabelą mówi wprost: „zbiornik słoneczny 100 l na dachu
   (czujnik kolektora, nie bojler pieca)”.

ZGODNOŚĆ Z ARCHIWUM CENTRALI (/archiwum)
----------------------------------------
                        centrala      aplikacja 0.31.37
  2026-10-23  dT        +6,2 °C       +6,3 °C
  2026-10-24  dT       -17,2 °C      -17,3 °C     1 pobranie -1,0
  2026-10-25  dT        -1,2 °C       -1,2 °C
  2026-10-26  dT        -3,8 °C       -3,7 °C
  2026-10-27  dT        +4,2 °C       +4,2 °C     2 pobrania -2,0
  2026-10-28  dT        -0,2 °C      ~0,0 °C      0 pobrań

Różnice 0,1–0,2 °C wynikają z tego, że aplikacja liczy dobę od LOKALNEJ
północy, a eksport z chmury, na którym liczyłem, kończył się 21:07.

Pobrania w całym miesiącu: 26 w 17 dniach (1–2 na dobę), każde 0,5–5 °C,
głównie po południu. Wcześniejsza reguła (>0,3 °C/min) dawała 1042
„pobrania” z szumu czujnika o rozdzielczości 0,1 °C.

CZEGO SIĘ SPODZIEWAĆ W APLIKACJI (28.10.2026)
---------------------------------------------
  „Spadek dziś · zmierzone”  ->  ok. -0,2 … -0,3°C   (NIE -17,5)
  „Pobrania ciepłej wody”    ->  brak
  Karta na pulpicie          ->  „SPADEK DZIŚ -0,3°C”, „POBRANIA DZIŚ —”

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.37"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja (nadpisuje 0.31.36, dane zostają):
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"

versionCode = 31037 (wyższy od 31036).

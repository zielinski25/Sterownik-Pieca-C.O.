Sterownik CO — wersja 0.31.36 (Android)
======================================
Poprawka do liczb w arkuszu "Panel słoneczny (solary)".

CO BYŁO ŹLE W 0.31.35 (widać to na zrzutach z 28.10.2026, godz. 23:09)
---------------------------------------------------------------------
1) "Spadek dziś · zmierzone" pokazywało -0,3 °C.
   Kod brał temperaturę KOLEKTORA (panel 17,5 -> 17,2 °C), a podpis pod tabelą
   mówi o wodzie w zbiorniku 100 l. Woda w zbiorniku zjechała tego dnia
   z 37,5 do 21,0 °C, czyli -16,5 °C.

2) "Pobrania ciepłej wody" pokazywało "24 razy · -7,2 °C", choć nikt wody nie brał.
   Stara reguła (>0,3 °C/min) łapała każde zwykłe zejście czujnika o 0,3 °C
   przy stygnięciu zbiornika. Dodatkowo 31,5 - 31,8 w typie Double daje
   -0,3000000000000007, czyli minimalnie więcej niż próg 0,30 - porównanie
   łapało granicę przez przypadek.

CO JEST W 0.31.36
-----------------
- "Zysk / Spadek dziś · zmierzone" liczone z czujnika ZBIORNIKA (tBojler).
- Pobranie wody = spadek PODTRZYMANY: woda musi zejść o co najmniej 0,5 °C
  od maksimum z ostatnich 5 minut i zostać niżej przez następne 5 minut.
  Krótkie drgnięcie czujnika wraca w ciągu minuty, więc nie jest liczone.
  Epizody bliższe niż 20 minut to jedno pobranie (kąpiel trwa), a spadek
  większy niż 15 °C to odczepiona sonda - obcinany.
- Ta sama reguła dla próbek lokalnych i dla historii z chmury.
- Stare, fałszywe flagi pobrań w pamięci telefonu są przeliczane od zera
  przy starcie aplikacji - nie trzeba niczego czyścić ani reinstalować.

SPRAWDZONE NA EKSPORCIE Z CHMURY (28 dni, 39 602 próbki)
--------------------------------------------------------
  26.10  43,5 -> 31,3 °C   zysk -12,2   pobrania 0
  27.10  31,3 -> 37,5 °C   zysk  +6,2   pobrania 2 razy, -6,5 °C
  28.10  37,5 -> 21,0 °C   zysk -16,5   pobrania 0
  Cały miesiąc: 32 pobrania w 16 dniach (wcześniej 1042 "pobrania" z szumu).

CZEGO SIĘ SPODZIEWAĆ W APLIKACJI (28.10.2026)
---------------------------------------------
  "Spadek dziś · zmierzone"  ->  -16,5°C
  "Pobrania ciepłej wody"    ->  brak
  Karta na pulpicie          ->  "SPADEK DZIŚ  -16,5°C", "POBRANIA DZIŚ  —"

UWAGA: liczba "dziś" liczy się od LOKALNEJ północy (00:00 czasu polskiego),
a nie od północy UTC. Dlatego 28.10 wychodzi -16,5 °C (37,5 -> 21,0),
a nie -15,8 °C liczone od 01:00.

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.36"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  "sdk.dir=" + ((Join-Path $env:LOCALAPPDATA "Android\Sdk") -replace '\\','/') | Set-Content local.properties -Encoding ASCII
  .\gradlew.bat :app:assembleDebug --console=plain

  Albo jedna komenda:  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Potem instalacja (nadpisuje 0.31.35, dane zostają):
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "$env:USERPROFILE\Downloads\SterownikCO-Android\SterownikCO-Android-v0.31.36\app\build\outputs\apk\debug\app-debug.apk"

versionCode = 31036 (wyższy od 31035, więc Android pozwoli nadinstalować).

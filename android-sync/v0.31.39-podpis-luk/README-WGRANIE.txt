Sterownik CO — wersja 0.31.39 (Android)
======================================
Temat 225: przerwy w danych na wykresie były ZAZNACZONE, ale BEZ PODPISU.

OBJAW (zrzut 2026-10-29 00:56, Wykresy, zakres 7 DNI)
------------------------------------------------------
Na wykresie widać cztery pionowe, przerywane kreski (dwie pary) — czyli przerwy
zostały wykryte. Ale ZADNA nie ma napisu „luka … min”. Puste miejsce zostaje
nieme: nie wiadomo, czy to brak próbek na 20 minut, czy na dwie godziny.

PRZYCZYNA — arytmetyka, nie dane
--------------------------------
Silnik rysował podpis tylko wtedy, gdy szerokość przerwy na ekranie była większa
od szerokości tekstu:

    if (xb - xa > tp.size.width + 6.dp) { drawText(...) }

Przy zakresie 7 dni cała doba ma ~590 px, więc 77 minut to około **4 px**,
a napis „luka 1 h 17 min” ma około **70 px**. Warunek nigdy nie był spełniony —
podpis znikał bez śladu. Przy powiększeniu do 6 H luka ma ~120 px i napis się
pojawiał, dlatego błąd nie wyszedł wcześniej.

CO JEST W 0.31.39
-----------------
1. Cała przerwa dostaje delikatne tło (10% przezroczystości, ten sam kolor co
   kreski). Widać ją przy KAŻDYM powiększeniu, bez czytania napisu.
2. Podpis trafia do środka przerwy, gdy się mieści. Gdy nie — staje obok kreski
   kończącej przerwę, dociągnięty do pola wykresu, żeby nie wyjechał za krawędź.
3. Etykiety pamiętają swoją prawą krawędź, więc przy kilku przerwach obok siebie
   druga wskakuje wiersz niżej zamiast się nakładać.

CZEGO SIĘ SPODZIEWAĆ
--------------------
- Zakres 7 DNI: luki widać jako jasne, pionowe pasy, a przy każdej stoi napis.
- Zakres 24 H / 6 H: napis „luka 1 h 17 min” w środku przerwy.
- Wykres 24.10 (dzień z luką 12:26 → 13:43) to pokaże. Twoje /archiwum podaje
  dla tego dnia pokrycie 93,3% (1344/1440) — luka istnieje naprawdę, po stronie
  centrali, a nie w aplikacji.

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.39"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31039 (wyższy od 31038).

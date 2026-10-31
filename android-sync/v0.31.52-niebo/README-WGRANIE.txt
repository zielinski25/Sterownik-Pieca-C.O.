SterownikCO — aplikacja 0.31.52  „model z niebem"
==================================================

CO SIE ZMIENILO (2 pliki)
-------------------------
1. android/app/build.gradle.kts
      versionCode 31051 -> 31052,  versionName "0.31.51" -> "0.31.52"

2. android/app/src/main/java/com/sterownikco/pro/core/SolarThermalModel.kt
      a) NOWY czlon "niebo" w bilansie zbiornika:
             C*dT/dt = Q - (tankUa + skyRadWk*(1-zachmurzenie)*noc)*(T-Ta)
         skyRadWk = 8.20 W/K, liczone tylko nocą (GTI 0 -> pelne, 50 W/m2 -> zero)
      b) reverseFlowKgS  0,004 -> 0,0
             rura heat-pipe to dioda termiczna, cyrkulacji wstecznej NIE MA
      c) Hour dostal pole cloudPct (zachmurzenie %), fromWeather je przekazuje
      d) tankUa ZOSTAJE 1,4 - model dzienny bez zmian

EFEKT (policzony na prawdziwych 11 nocach 18-28.10, kod z tej paczki)
---------------------------------------------------------------------
      stary model : sredni blad 2,07 C
      nowy  model : sredni blad 0,88 C
      poprawa     : 58%
      przy braku danych o chmurach (NaN) - model liczy jak przed 0.31.52

CO SIE ZMIENI NA EKRANIE
------------------------
- Karta DZIS / "do konca dnia": wieczorem woda stygnie teraz Wiecej niz pokazywala,
  zwlaszcza przy czystym niebie. To jest poprawka, nie regres.
- Szczyty dzienne: przy czystym niebie nizej o 0,4-3,3 C, przy pelnym
  zachmurzeniu bez zmian.

JAK WGRAC
---------
Skopiuj folder "android" z tej paczki na projekt:
    C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android
(2 pliki sie nadpisza, reszta projektu zostaje)

Potem:
    cd C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android
    .\gradlew.bat assembleDebug

APK: android\app\build\outputs\apk\debug\app-debug.apk

JAK SPRAWDZIC, ZE DZIALA
------------------------
1. W ustawieniach ma byc "v0.31.52".
2. Wieczorem przy czystym niebie karta "do konca dnia" pokaze spadek
   wiekszy niz w 0.31.51 - i ten spadek bedzie blizej prawdy.
3. Jesli chmury sa na 100%, wynik ma byc prawie taki sam jak w 0.31.51.

CZEGO NIE ZMIENIONO
-------------------
Firmware centrali: bez zmian (3.56.7 zostaje, 3.56.8 nadal czeka).
Reszta aplikacji: bez zmian.

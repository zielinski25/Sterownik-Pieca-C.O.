SterownikCO — aplikacja 0.31.53  "pompa na cala wysokosc"
=========================================================

WERSJA PODBITA: 0.31.52 -> 0.31.53  (versionCode 31053)

Paczka ma 3 pliki. Zawiera model z 0.31.52 (jesli go nie wgrywales) ORAZ
poprawke wykresu pompy.


CO SIE ZMIENILO
---------------
1) WYKRES — praca pompy wraca na CALA WYSOKOSC   [ui/chart/ChartEngine.kt]
   Przed: pompa byla niskim paskiem 10 dp przy samej dolnej krawedzi
          (tak zrobil 0.31.34) — wygladalo jak kreski "pierdki".
   Teraz: praca pompy to pelna, polprzezroczysta kolumna od dolu do gornej
          krawedzi wykresu. Widac ja tak samo wyraznie jak w wersjach
          przed 0.31.34.
   Kolumny sa pod temperaturami (18% krycia), wiec nie zaslaniaja linii
   temperatur — dlatego nie ma juz powodu, dla ktorego 0.31.34 je zwezalo.
   W trybie "tylko POMPA" (bez temperatur) zostaje zwykla linia na pelnej
   wysokosci — bez zmian.

2) MODEL Z NIEBEM — to samo, co w 0.31.52   [core/SolarThermalModel.kt]
   Czlon "niebo" w bilansie zbiornika:
       C*dT/dt = Q - (tankUa + skyRadWk*(1-zachmurzenie)*noc)*(T-Ta)
       tankUa = 1.4 W/K (bez zmian), skyRadWk = 8.20 W/K
       reverseFlowKgS = 0.0  (bylo 0.004 - rura heat-pipe to dioda)
   Efekt na 11 prawdziwych nocach: blad 2.07 -> 0.88 C (58% lepiej).

3) WERSJA   [app/build.gradle.kts]
   31053 / "0.31.53"


CZEGO NIE ZMIENIONO
-------------------
- Nazwy czujnikow: "Panel sloneczny" / "Kolektor" — ZOSTAJA, jak chciales.
  (Wersja 0.31.53 z nazwami "zbiornik" zostala wycofana i NIE jest ta paczka.)
- Firmware centrali: bez zmian. 3.56.7 zostaje, 3.56.8 nadal czeka.
- Reszta aplikacji: bez zmian.


JAK WGRAC
---------
Skopiuj folder "android" z tej paczki na projekt:
    C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android
(3 pliki sie nadpisza, reszta projektu zostaje)

Potem:
    cd C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android
    .\gradlew.bat assembleDebug

APK: android\app\build\outputs\apk\debug\app-debug.apk


JAK SPRAWDZIC, ZE DZIALA
------------------------
1. Ustawienia -> na dole "v0.31.53".
2. Wykresy -> zakres 30 dni -> wlacz grupy TEMPERATURY i POMPA.
3. Praca pompy ma byc zielona kolumna od dolu DO GORNEJ KRAWEDZI wykresu,
   a nie krotka kreska przy dolnej osi.
4. Linie temperatur maja byc widoczne NAD kolumnami (kolumna jest przezroczysta).

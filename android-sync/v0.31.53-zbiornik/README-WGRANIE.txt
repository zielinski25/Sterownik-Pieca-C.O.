SterownikCO — aplikacja 0.31.53  „zbiornik, nie panel"
=======================================================

WERSJA PODBITA: 0.31.52 -> 0.31.53  (versionCode 31053)

Ta paczka zawiera WSZYSTKO z 0.31.52 i poprawki nazw. Jesli nie wgrywales
0.31.52 — nie musisz, ta paczka ja zastepuje.


CO SIE ZMIENILO — 12 plikow + wersja
------------------------------------
CZESC 1: MODEL (to samo co 0.31.52)
  core/SolarThermalModel.kt
      a) czlon "niebo": C*dT/dt = Q - (tankUa + skyRadWk*(1-zachmurzenie)*noc)*(T-Ta)
         skyRadWk = 8.20 W/K, tylko noc (GTI 0 -> pelne, 50 W/m2 -> zero)
      b) reverseFlowKgS 0,004 -> 0,0   (rura heat-pipe to dioda termiczna)
      c) Hour.cloudPct — zachmurzenie z prognozy, ktora aplikacja juz pobierala
      d) tankUa ZOSTAJE 1,4 — model dzienny nietkniety
      Efekt: blad nocny 2,07 -> 0,88 C (58% lepiej) na 11 prawdziwych nocach.

CZESC 2: NAZWY — koniec z "kolektorem" i "glikolem"
  Czujnik t_panel mierzy TEMPERATURE WODY w zbiorniku 100 l na dachu.
  W instalacji nie ma glikolu (woda z wodociagu) i nie ma drugiego obiegu.
  Poprawione napisy:

  core/Sensors.kt        "Panel sloneczny" / "Kolektor glikolowy"  ->  "Zbiornik solarny (100 l na dachu)" / "Czujnik w wodzie zbiornika"
  core/TileSpecs.kt      kafel "Panel sloneczny" + "Temperatura kolektora" -> "Zbiornik solarny" + "Temperatura wody w zbiorniku 100 l"
  core/TileSpecs.kt      kafel na pulpicie "Panel slon." -> "Zbiornik"; opis "naslonecznienie ok" -> "dzien"
  core/TileSpecs.kt      "alarmy kotla i kolektora" -> "kotla i zbiornika solarnego"
  core/Telemetry.kt      legenda wykresu "Panel" -> "Zbiornik"
  core/AppModel.kt       Telegram: "*Panel:*" -> "*Zbiornik:*", "ALARM PANELU" -> "ALARM ZBIORNIKA SOLARNEGO"
  ui/screens/Weather.kt  "PANEL - POWIETRZE" -> "ZBIORNIK - POWIETRZE"
  ui/screens/Overlays.kt "Kolektor sloneczny prozniowy" -> "Zbiornik solarny 100 l (na dachu)"
  ui/screens/Lists.kt    "Przegrzanie kotla & panelu" -> "... & zbiornika"; lista czujnikow "Panel" -> "Zbiornik"
  ui/screens/Dashboard.kt  "PRZEGRZANIE PANELU" -> "PRZEGRZANIE ZBIORNIKA"
  ui/screens/sheets/Sensors.kt  "Panel teraz" -> "Zbiornik teraz", "Roznica panel - powietrze" -> "zbiornik - powietrze",
                                "(czujnik kolektora...)" -> "(czujnik w wodzie zbiornika...)"
  widget/PiecWidget.kt   widget "PANEL SLONECZNY" -> "ZB. SOLARNY", skrot "PANEL" -> "SOLAR", zakres 0-100 C
  res/values/strings.xml opis widgetu "bojler i panel sloneczny" -> "bojler i zbiornik solarny"

CZESC 3: ZNALEZIONY BLAD (poprawiony)
  ui/screens/Dashboard.kt — przy alarmie przegrzania panelu aplikacja pokazywala
  temperature BOJLERA PIECA (S.t_bojler) zamiast zbiornika solarnego (S.t_panel).
  Teraz pokazuje wlasciwa liczbe.


CO SIE NIE ZMIENILO
-------------------
- Firmware centrali: bez zmian. 3.56.7 zostaje, 3.56.8 nadal czeka.
- Klucze w bazie ("panel", "t_panel"): bez zmian — historia i wykresy zostaja.
- Reszta aplikacji: bez zmian.
- Zakresy liczbowe czujnika (0-140 C) w modelu: bez zmian, zeby nie ruszac
  walidacji probek. Zmienil sie tylko pasek na kafelku (0-100 C).


JAK WGRAC
---------
Skopiuj folder "android" z tej paczki na projekt:
    C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android
(13 plikow sie nadpisze, reszta projektu zostaje)

Potem:
    cd C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android
    .\gradlew.bat assembleDebug

APK: android\app\build\outputs\apk\debug\app-debug.apk


JAK SPRAWDZIC, ZE DZIALA
------------------------
1. Ustawienia -> na dole ma byc "v0.31.53".
2. Kafelek na pulpicie nazywa sie teraz "Zbiornik" (wczesniej "Panel slon.").
3. Arkusz po kliknieciu kafelka: "Zbiornik solarny 100 l (na dachu)".
4. Wykresy: legenda "Zbiornik" zamiast "Panel".
5. Widget: "ZB. SOLARNY" / skrot "SOLAR".

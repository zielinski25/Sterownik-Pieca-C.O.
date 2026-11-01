SterownikCO — CAŁY PROJEKT ANDROID, wersja 0.31.54
==================================================

To jest KOMPLETNY projekt, gotowy do zbudowania. Nie składasz nic z kawałków:
nie kopiujesz pojedynczych plików, nie nadpisujesz nic w starym folderze.
Rozpakowujesz w nowym miejscu i budujesz.

Dlaczego tak: poprzednie paczki były "kawałkami" (3 pliki, 12 plików, 14 plików)
i właśnie na tym się myliłem — dwie różne paczki dostały ten sam numer 0.31.53,
a Ty zbudowałeś nie tę. Tutaj numer jest jeden, a w środku wszystko.


CO JEST W ŚRODKU
----------------
144 pliki, dokładnie tyle, ile jest w repozytorium — sprawdzone plik po pliku:
  96 plików .kt (cały kod aplikacji)
  res/ (ikony, motywy, teksty, strings.xml)
  build.gradle.kts, settings.gradle.kts, gradle.properties
  gradlew, gradlew.bat, gradle/wrapper/ (wrapper w środku — bez niego build nie ruszy)
  AndroidManifest.xml, proguard, README projektu

WERSJA: versionCode 31054, versionName "0.31.54"
Sprawdź w Ustawieniach po zbudowaniu: musi być "v0.31.54".


CO JEST W TEJ WERSJI
--------------------
1) Stare nazwy — dokładnie te, które były od pierwszej wersji:
     kafelek              "Panel słon."
     Ustawienia           "Panel słoneczny" / "Temperatura kolektora"
     po kliknięciu        "Kolektor słoneczny próżniowy"
     arkusz               "Panel teraz / powietrze", "Różnica panel − powietrze"
     wykresy              "Panel"
     Pogoda               "PANEL − POWIETRZE"
     widget               "PANEL SŁONECZNY" / "PANEL"
     Telegram             "*Panel:*", "ALARM PANELU"
     alarmy               "Przegrzanie kotła & panelu", baner "PRZEGRZANIE PANELU"
   Słowo "Zbiornik" nie występuje w żadnym tekście na ekranie (sprawdzone).

2) Wykres: praca pompy na CAŁĄ wysokość (półprzezroczysta kolumna pod temperaturami).

3) Model z niebem: skyRadWk 8,20 W/K, tankUa 1,4 — bez zmian od 0.31.52.


JAK ZBUDOWAĆ (3 kroki, bez kopiowania plików)
---------------------------------------------
1. Rozpakuj ten ZIP do nowego folderu, np.:
      C:\Users\Rafcio\Documents\SterownikCO-0.31.54

2. Jedna linia w PowerShellu:
      cd C:\Users\Rafcio\Documents\SterownikCO-0.31.54; .\gradlew.bat assembleDebug

3. APK będzie tutaj:
      C:\Users\Rafcio\Documents\SterownikCO-0.31.54\app\build\outputs\apk\debug\app-debug.apk


JEŚLI BUILD SIĘ NIE CHCE
------------------------
- "SDK location not found" → otwórz ten folder w Android Studio raz
  (samo otwarcie utworzy local.properties), potem buduj znowu.
- JDK musi być 17–21. Android Studio ma swoje — najpewniej budować z niego.
- Pierwszy build pobiera zależności z internetu, może potrwać kilka minut.


JAK SPRAWDZIĆ, ŻE MASZ DOBRĄ WERSJĘ (4 punkty)
----------------------------------------------
1. Ustawienia -> na dole "v0.31.54".  Jeśli "v0.31.53" — to nie ta paczka.
2. Kafelek na pulpicie: "Panel słon."   (NIE "Zbiornik")
3. Klik w kafelek: tytuł "Kolektor słoneczny próżniowy".
4. Wykresy: legenda "Panel", praca pompy jako kolumna na całą wysokość.


CZEGO NIE RUSZA
---------------
Firmware centrali: bez zmian. 3.56.7 zostaje. 3.56.8 czeka, nie wgrany.
Stary folder SterownikCO-0.31.49 możesz zostawić — ten ZIP go nie dotyka.

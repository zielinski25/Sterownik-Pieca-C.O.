SterownikCO — aplikacja 0.31.54  "STARE NAZWY"
=============================================

WERSJA PODBITA: 0.31.53 -> 0.31.54  (versionCode 31054)

DLACZEGO NOWY NUMER
-------------------
Dzisiaj powstały DWIE rózne paczki z tym samym numerem 0.31.53:
  - 0.31.53-ZBIORNIK  (nazwy "Zbiornik")  <- pomylka, wycofana
  - 0.31.53-POMPA     (stare nazwy)
W Ustawieniach obie pokazywały "v0.31.53", więc nie dało się ich odróżnić.
Teraz jest 0.31.54 i to jest JEDYNA aktualna wersja.


CO ROBI
-------
1) Przywraca nazwy sprzed lat — dokładnie te, które były w aplikacji od
   pierwszej wersji:
      kafelek na pulpicie      "Panel słon."
      lista w Ustawieniach     "Panel słoneczny", opis "Temperatura kolektora"
      czujnik                  "Panel słoneczny (solary)"
                               "Kolektor glikolowy · 1-Wire / PT1000"
      arkusz po kliknięciu     "Kolektor słoneczny próżniowy"
      wiersze w arkuszu        "Panel teraz / powietrze"
                               "Różnica panel − powietrze"
      legenda wykresów         "Panel"
      ekran Pogoda             "PANEL − POWIETRZE"
      widget                   "PANEL SŁONECZNY", skrót "PANEL"
      Telegram                 "*Panel:*", "ALARM PANELU"
      alarmy w Ustawieniach    "Przegrzanie kotła & panelu", lista "... Piec, Panel ..."
      baner alarmu             "PRZEGRZANIE PANELU"

2) Zawiera wykres z 0.31.53-pompa: praca pompy na CAŁĄ wysokość wykresu
   (półprzezroczysta kolumna pod temperaturami).

3) Model z niebem (0.31.52) — bez zmian: skyRadWk 8,20 W/K, tankUa 1,4.


DLACZEGO 14 PLIKÓW, A NIE 3
---------------------------
Zła paczka zmieniła 12 plików. Gdyby teraz przyszły tylko 3 pliki (wykres),
złe nazwy zostałby w projekcie. Dlatego paczka nadpisuje WSZYSTKIE pliki,
które tamta ruszyła — 12 + 2 (wykres i model) = 14.


JAK WGRAC
---------
Skopiuj folder "android" z tej paczki na projekt, zgadzając się na zamianę:
    C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android

Potem jedna linia w PowerShellu:
    cd C:\Users\Rafcio\Documents\SterownikCO-0.31.49\android; .\gradlew.bat assembleDebug

APK: android\app\build\outputs\apk\debug\app-debug.apk


JAK SPRAWDZIĆ, ŻE MASZ DOBRĄ WERSJĘ (4 punkty)
----------------------------------------------
1. Ustawienia -> na dole "v0.31.54".  Jeśli "v0.31.53" — to zła, stara paczka.
2. Kafelek na pulpicie: "Panel słon."   (NIE "Zbiornik")
3. Klik w kafelek: tytuł "Kolektor słoneczny próżniowy".
4. Wykresy: legenda "Panel", a praca pompy to kolumna na całą wysokość.


CZEGO NIE RUSZA
---------------
Firmware centrali: bez zmian. 3.56.7 zostaje. 3.56.8 nadal czeka, nie wgrane.

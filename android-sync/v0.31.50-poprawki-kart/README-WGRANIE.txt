STEROWNIK CO — NAKLADKA 0.31.50 (5 plikow)
=========================================

DLA KOGO: masz zainstalowana wersje 0.31.47 (albo starsza).

CO NAPRAWIA

1. Karta "DZIŚ" w prognozie 7-dniowej (poprawka z 0.31.48)
   Bylo: na jednym ekranie "ZYSK DZIŚ +1.5", "DO KOŃCA DNIA +2.1"
         i "DZIŚ -2.4". Trzy liczby, ktore nie moga byc prawdziwe razem.
   Jest: "DZIŚ" = "ZYSK DZIŚ" + "DO KOŃCA DNIA".

2. Karta "DO KOŃCA DNIA" przy chlodzeniu (NOWE w 0.31.50)
   Bylo: gdy model liczyl, ze woda stygnie, karta pokazywala "+0.0 °C",
         a karta DZIŚ obok ujemna liczbe. Sprzecznosc na ekranie.
   Jest: pokazuje realna zmiane, np. "-2.8 °C".
   Symulacja: dotyczylo 12.8% dob. Po poprawce 0.0%.

3. Kafelek POMIESZCZENIE (NOWE w 0.31.50)
   Bylo: podpis "komfort cieplny" wpisany na sztywno, takze przy 14.5 °C.
   Jest: progi -> ponizej 16 "zimno", 16-19 "chlodno",
         19-24 "komfort cieplny", powyzej 24 "cieplo".

4. Szybsze rysowanie wykresow (z 0.31.49)
   Mniej obiektow tworzonych przy kazdej klatce.

CO JEST W SRODKU (5 plikow, nic wiecej)
    app/build.gradle.kts                                      versionCode 31050
    app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
    app/src/main/java/com/sterownikco/pro/core/TileSpecs.kt
    app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
    app/src/main/java/com/sterownikco/pro/ui/chart/ChartEngine.kt

To jest KOMPLET plikow, ktore roznia 0.31.47 od 0.31.50 (sprawdzone
poleceniem git diff po calym katalogu android/). Podmieniasz je w calosci,
nadpisujac stare.

JAK WGRAC
1. Rozpakuj ZIP. W srodku sa sciezki od app/...
2. Skopiuj katalog app/ do katalogu swojego projektu, zgadzajac sie na
   nadpisanie 5 plikow.
3. Android Studio: Build -> Clean Project, potem Build -> Rebuild Project.
4. Uruchom na telefonie (Run).
5. Wiecej -> "Zainstalowane oprogramowanie" -> "APLIKACJA WEB PRO"
   ma pokazac v0.31.50.

JAK SPRAWDZIC, ZE DZIALA
Wejdz w Pogoda -> Panel sloneczny (solary) -> "PROGNOZA POTENCJALU
SOLARNEGO NA 7 DNI". Karta "DZIŚ" ma byc suma "ZYSK DZIŚ" i
"DO KOŃCA DNIA". Przyklad z 29.10.2026: 1.5 + 2.1 = +3.6 (nie -2.4).

Kafelek POMIESZCZENIE przy 14.5 °C ma pokazac "zimno".

CZEGO NIE ROBI
Nie zmienia firmware centrali. Nie zmienia ustawien. Nie rusza historii
ani bufora offline.

GDY SIE NIE KOMPILUJE
To znaczy, ze baza jest starsza niz 0.31.47. Wez wtedy PELNY PROJEKT 0.31.49
z Dysku i naloz na niego te 5 plikow.

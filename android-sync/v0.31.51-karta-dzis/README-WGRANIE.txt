STEROWNIK CO — NAKLADKA 0.31.51 (5 plikow)
=========================================

DLA KOGO: masz zainstalowana wersje 0.31.47 (albo starsza).
Jesli masz juz 0.31.50 — podmien tylko Weather.kt i build.gradle.kts.

CO NAPRAWIA

1. Karta "DZIŚ" w prognozie 7-dniowej (z 0.31.48)
   Bylo: "ZYSK DZIŚ +1.5", "DO KOŃCA DNIA +2.1" i "DZIŚ -2.4" — trzy
         liczby, ktore nie mogly byc prawdziwe razem.
   Jest: "DZIŚ" = "ZYSK DZIŚ" + "DO KOŃCA DNIA".

2. Karta "DO KOŃCA DNIA" po zachodzie slonca (NOWE w 0.31.51)
   Bylo: po zachodzie pokazywala "—", a karta DZIŚ obok liczbę ujemna,
         np. "ZYSK DZIŚ +0.2", "—", "DZIŚ -1.0". Nie dalo sie sprawdzic,
         skad ta roznica.
   Jest: pokazuje nocne stygniecie wody do polnocy, np. "-1.2 °C",
         z podpisem "bez slonca · woda stygnie do polnocy".
         Wtedy +0.2 i -1.2 daje -1.0. Wszystko sie zgadza.

3. Karta "DO KOŃCA DNIA" przy chlodzeniu za dnia (z 0.31.50)
   Bylo: "+0.0 °C" gdy model liczyl, ze woda stygnie.
   Jest: realna wartosc, np. "-2.8 °C".

4. Kafelek POMIESZCZENIE (z 0.31.50)
   Bylo: podpis "komfort cieplny" wpisany na sztywno, takze przy 14.5 °C.
   Jest: ponizej 16 "zimno", 16-19 "chlodno", 19-24 "komfort cieplny",
         powyzej 24 "cieplo".

5. Szybsze rysowanie wykresow (z 0.31.49)

CO JEST W SRODKU (5 plikow, nic wiecej)
    app/build.gradle.kts                                      versionCode 31051
    app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
    app/src/main/java/com/sterownikco/pro/core/TileSpecs.kt
    app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
    app/src/main/java/com/sterownikco/pro/ui/chart/ChartEngine.kt

JAK WGRAC
1. Rozpakuj ZIP. W srodku sa sciezki od app/...
2. Skopiuj katalog app/ do katalogu swojego projektu, zgadzajac sie na
   nadpisanie 5 plikow.
3. Android Studio: Build -> Clean Project, potem Build -> Rebuild Project.
4. Uruchom na telefonie (Run).
5. Wiecej -> "Zainstalowane oprogramowanie" -> "APLIKACJA WEB PRO"
   ma pokazac v0.31.51.

JAK SPRAWDZIC
Wieczorem, po zachodzie slonca: karta "DO KOŃCA DNIA" ma pokazac liczbe
ujemna z podpisem "woda stygnie do polnocy", a nie "—".
Karta "DZIŚ" ma byc suma dwoch gornych kartek.

CZEGO NIE ROBI
Nie zmienia firmware centrali. Nie zmienia ustawien. Nie rusza historii
ani bufora offline. Nie zmienia modelu fizycznego.

UWAGA O DOKLADNOSCI
Model fizyczny zaniza nocne stygniecie ok. 2x po slonecznym dniu
(sprawdzone na Twoim archiwum). Prognoza "calej doby" moze byc przez to
o 1-2 °C za optymistyczna. Karta jest podpisana "szacunek, nie pomiar".
Naprawa wymaga modelu warstwowego zbiornika — osobna, wieksza zmiana.

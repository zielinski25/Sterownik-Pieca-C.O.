STEROWNIK CO — NAKLADKA 0.31.49 (4 pliki)
=========================================

DO CZEGO TO JEST
Ten pakiet podmienia 4 pliki w Twoim projekcie Androida i podnosi go do
wersji 0.31.49. Zawiera poprawke, ktorej NIE MA w tym, co masz teraz na
telefonie: karta "DZIŚ" w prognozie 7-dniowej liczona jest modelem fizycznym,
tym samym co karta "DO KOŃCA DNIA".

Objaw starej wersji: na jednym ekranie stoi
    ZYSK DZIŚ               +1.5 °C
    DO KOŃCA DNIA           +2.1 °C
    DZIŚ (karta 7 dni)      -2.4 °C      <-- nieprawda, powinno być +3.6

CO JEST W SRODKU (4 pliki, nic wiecej)
    app/build.gradle.kts                                    versionCode 31049
    app/src/main/java/com/sterownikco/pro/core/SolarAnalytics.kt
    app/src/main/java/com/sterownikco/pro/ui/screens/Weather.kt
    app/src/main/java/com/sterownikco/pro/ui/chart/ChartEngine.kt

WAZNE: to NIE sa pliki "tylko zmienione od 0.31.48". To komplet plikow
odpowiedzialnych za panel solarny i rysowanie wykresow. Podmieniasz je
w calosci, nadpisujac stare.

JAK WGRAC
1. Rozpakuj ZIP. W srodku sa sciezki od app/...
2. Skopiuj katalog app/ do katalogu swojego projektu, zgadzajac sie na
   nadpisanie 4 plikow.
3. W Android Studio: Build -> Clean Project, potem Build -> Rebuild Project.
4. Uruchom na telefonie (Run).
5. W aplikacji: Wiecej -> "Zainstalowane oprogramowanie" -> wiersz
   "APLIKACJA WEB PRO" ma pokazac v0.31.49.

JAK SPRAWDZIC, ZE POPRAWKA DZIALA
Wejdz w Pogoda -> Panel sloneczny (solary) -> sekcja
"PROGNOZA POTENCJALU SOLARNEGO NA 7 DNI".
Karta "DZIŚ" ma pokazac sume: "ZYSK DZIŚ" + "DO KOŃCA DNIA".
Przyklad z 29.10.2026 13:19: 1.5 + 2.1 = +3.6 °C (a nie -2.4 °C).

CZEGO NIE ROBI
Nie zmienia niczego w firmware centrali. Nie zmienia ustawien. Nie rusza
historii ani bufora offline.

GDY COS NIE DZIALA
Jesli projekt nie kompiluje sie po podmianie, to znaczy, ze Twoja baza jest
starsza niz 0.31.47. Wtedy wez PELNY PROJEKT 0.31.49 z Dysku.

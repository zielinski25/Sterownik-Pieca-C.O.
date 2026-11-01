Sterownik CO - aplikacja Android 0.31.71 (pelny projekt)
=========================================================

Co nowego w 0.31.71
-------------------
W 0.31.70 zniknal CZARNY pas przy wycieciu kamery (potwierdzone na telefonie
zrzutem 12:20: zero pikseli #000000 po lewej). Zostal jednak widoczny SZEW:
przez 37 dp od lewej krawedzi bylo tlo aplikacji (Pal.Bg #060C18), a dopiero
za nim szyna nawigacji (gradient #081421 -> #0B1725). Dwa odcienie obok
siebie = pionowa linia przy krawedzi, a szyna wygladala na odsuneita od brzegu.

CO ZMIENIONE
------------
1. ui/components/Chrome.kt - NavRail ma nowy parametr startInset (domyslnie 0):

       Modifier.width(Dimens.railW + startInset)
       ...padding(start = 8.dp + startInset, ...)

   Czyli szyna POCHLANIA wciecie: jej tlo (gradient + obramowanie) dochodzi do
   samej krawedzi ekranu, a ikony i podpisy zaczynaja sie dopiero za wycieciem.
   Jeden ciagły pasek, bez szwu, nic pod obiektywem.

2. ui/SterownikApp.kt - wciecie liczone z systemu, nie ze stalej:

       val cutoutStart = WindowInsets.displayCutout.asPaddingValues()
           .calculateStartPadding(LocalLayoutDirection.current)

   i przekazane do szyny: NavRail(page, startInset = cutoutStart).
   Tresc obok szyny NIE dostaje juz zadnego paddingu (w trybie szyny wciecie
   jest juz zuzyte przez szyne). W trybie dolnego paska zostaje dokladnie to
   co w 0.31.70 (displayCutoutPadding na tresci), wiec pion sie nie zmienia.

CZEGO NIE ZMIENIONO
-------------------
- Motyw (values-v27/themes.xml) i ustawienie w MainActivity z 0.31.70 - bez zmian.
- Uklad z 0.31.69 (szyna gdy wysokosc < 480 dp i szerokosc >= 600 dp) - bez zmian.
- Pasek statusu i pasek nawigacji Androida - elementy systemu, nie ruszamy.

WERSJA
------
versionCode 31071 / versionName 0.31.71

JAK ZBUDOWAC (PowerShell, z rozpakowanego katalogu)
---------------------------------------------------
    .\gradlew.bat --no-daemon testDebugUnitTest assembleDebug

APK: app\build\outputs\apk\debug\app-debug.apk

UWAGA: kod NIE byl kompilowany tam, gdzie powstal (brak Javy i Gradle).
Sprawdzone statycznie: narzedzia/sprawdz_uklad.py daje 37 OK / 0 BŁĄD.
Pierwsza kompilacja jest u Ciebie.

CO SPRAWDZIC PO WGRANIU
-----------------------
Telefon w poziomie: szyna nawigacji ma dochodzic do SAMEJ lewej krawedzi,
bez jasniejszego czy ciemniejszego paska obok. Ikony maja byc odsuniete od
kamerki. Telefon w pionie: bez zmian wzgledem 0.31.70.

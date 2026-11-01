Sterownik CO - aplikacja Android 0.31.70 (pelny projekt)
=========================================================

Co nowego w 0.31.70
-------------------
Po lewej stronie ekranu w poziomie zostawal CZARNY PAS szerokosci 110 px
(37 dp). Nie byl to juz uklad aplikacji — 0.31.69 usunela ramke 412 dp
i aplikacja zajmuje cale okno. Ten pas to obszar WYCIECIA EKRANU (kamera
w ekranie, tzw. punch-hole) zamalowany przez system.

DOWOD ZE TO SYSTEM, NIE APLIKACJA
---------------------------------
Kolor pasma to czysta czern #000000. Zadne tlo aplikacji nie ma tego koloru:
  Pal.Bg #060D18, Pal.Nav #081421, Pal.Top #071421, @color/bg #060D18.
Gdyby pas byl czescia naszego okna, mialby kolor Pal.Bg. Jest CZARNY, wiec
lezy POZA oknem aplikacji — to system rezerwuje go na wyciecie.

Ten sam pas byl w 0.31.68 (tez 110 px), tylko niewidoczny, bo obok byla
duzo wieksza czarna ramka.

PRZYCZYNA
---------
Motyw nie mial ustawionego windowLayoutInDisplayCutoutMode, wiec dzialal
tryb domyslny (DEFAULT): pod wycieciem wolno rysowac TYLKO w pionie.
W poziomie system omija wyciecie i zamalowuje jego obszar na czarno
(letterbox). Dotyczy to kazdej aplikacji, ktora tego nie zadeklaruje.

CO ZMIENIONE
------------
1. res/values-v27/themes.xml (nowy plik) - ten sam motyw co dotad plus:

       <item name="android:windowLayoutInDisplayCutoutMode">shortEdges</item>

   "shortEdges" = wolno rysowac pod wycieciem zarowno w pionie, jak i
   w poziomie (wyciecie jest na krotkiej krawedzi). Czarny pas znika.
   Atrybut istnieje od API 27, dlatego katalog values-v27.

2. MainActivity.kt - to samo ustawione w kodzie, przed pierwszym rysowaniem:

       window.attributes.layoutInDisplayCutoutMode =
           WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES

   Powod: czesc urzadzen (m.in. Samsung) ignoruje sam atrybut z motywu.
   Ustawienie chronione warunkiem SDK_INT >= P (stala istnieje od API 28;
   minSdk aplikacji to 26, wiec bez tego warunku starsze telefony by padly).

3. ui/SterownikApp.kt - interfejs odsunięty od samej dziurki:

       Row(Modifier.fillMaxSize().displayCutoutPadding()) { ... }

   Obszar wyciecia jest teraz czescia okna i ma tlo Pal.Bg, a zadne ikony
   ani napisy nie wchodza pod obiektyw. W pionie ten padding wynosi 0
   (wyciecie jest nad oknem), wiec uklad w pionie sie nie zmienia.

CZEGO SWIADOMIE NIE ZMIENIONO
-----------------------------
- Pasek statusu (37 dp u gory) i pasek nawigacji Androida (45 dp po prawej)
  zostaja systemowe i w kolorach motywu. To nie sa nasze elementy.
- Uklad z 0.31.69 (szyna nawigacji w niskim i szerokim oknie) bez zmian.
- Brak blokady orientacji.

JAK SPRAWDZIC
-------------
Po wgraniu, telefon w poziomie: pas po lewej ma byc w kolorze tla aplikacji
(ciemny granat) i zadna ikona nie moze byc pod kamerka. W pionie wyglad
bez zmian.

JESLI PAS ZOSTANIE CZARNY
-------------------------
Wtedy to nie wyciecie, tylko inny element systemu (np. drugi ekran lub
panel boczny). Potrzebny bedzie zrzut w pionie i wersja Androida —
bez tego nie zgaduję.

WERSJA
------
versionCode 31070 / versionName 0.31.70

JAK ZBUDOWAC (PowerShell, z rozpakowanego katalogu)
---------------------------------------------------
    .\gradlew.bat --no-daemon testDebugUnitTest assembleDebug

APK: app\build\outputs\apk\debug\app-debug.apk

UWAGA: kod NIE byl kompilowany w srodowisku, w ktorym go napisano (brak
Javy i Gradle). Sprawdzone statycznie: narzedzia/sprawdz_uklad.py daje
30 OK / 0 BŁĄD. Pierwsza kompilacja jest u Ciebie.

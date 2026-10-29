═══════════════════════════════════════════════════════════
  ANDROID v0.31.49 — SZYBSZE RYSOWANIE WYKRESU
  2026-10-29
═══════════════════════════════════════════════════════════

WERSJA PODBITA: TAK (0.31.48 -> 0.31.49, versionCode 31048 -> 31049)

CO ZMIENIONO — 2 pliki
------------------------
1) ui/chart/ChartEngine.kt
   Przeniesione poza petle rysowania: kolor serii (hexColor),
   pedzel gradientu pod linią, PathEffect kreskowania i obiekt Stroke.
   Wczesniej kazdy z tych obiektow powstawal DLA KAZDEGO ODCINKA
   i PRZY KAZDYM odrysowaniu klatki. Przy 6 seriach i kilkudziesieciu
   odcinkach to setki alokacji shaderow na klatke — czyli widoczne
   zacinki przy przesuwaniu i przy otwieraniu wykresu.
   Teraz: jeden pedzel, jedna kreska, jedno pioro na cala klatke.

2) app/build.gradle.kts
   versionCode 31049, versionName "0.31.49"

CZY BUDOWA SIE UDA
------------------
Nie mam w tym srodowisku Javy ani Androida, wiec NIE zbudowalem tego
sam. Sprawdzone statycznie:
  - nawiasy { } i ( ) bilansuja sie dokladnie tak samo jak przed zmiana
    (171/171 i 611/611)
  - nie ma juz odwolan do usunietej zmiennej "dash"
  - kazda uzywana nazwa (accent, fillBrush, dashFx, strokeSolid,
    strokeDash) jest zadeklarowana PRZED miejscem uzycia

Jesli build sie wysypie, napisz mi tresc bledu — poprawka bedzie
jednolinijkowa.

JAK ZBUDOWAC
-------------
powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1
albo w Android Studio: Build -> Build Bundle(s)/APK(s) -> Build APK(s)

CO ZOSTALO Z POPRZEDNICH WERSJI
--------------------------------
Nic nie usuniete. Zmiana jest wylacznie wydajnosciowa — wyglad wykresu,
kolory, kreskowanie odcinkow symulowanych, podpisy osi i legenda sa
identyczne jak w 0.31.48.

CZEGO TA WERSJA NIE NAPRAWIA
-----------------------------
Dziur w wykresie. One nie powstaja w aplikacji, tylko w centrali —
powod opisany osobno (kanal "Ogrz. sr." nie ma wartosci przez pierwsze
~20 minut po restarcie pieca). To wymaga zmiany w firmware, nie w APK.

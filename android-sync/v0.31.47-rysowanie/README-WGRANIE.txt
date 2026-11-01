Sterownik CO — Android 0.31.47 „rysowanie wykresu"
==================================================

CO TO ZA WERSJA
---------------
0.31.46 skróciło POBieranie historii. Zostało RYSOwanie — i to ono
odpowiadało za to, że wykresy dalej nie były natychmiastowe.
0.31.47 zmienia wyłącznie sposób rysowania. Wygląd wykresu jest ten sam.

CO DOKŁADNIE BYŁO NIE TAK (prostymi słowami)
--------------------------------------------
1) Każda pojedyncza próbka z historii była zamieniana na osobny punkt
   krzywej PRZY KAŻDYM odrysowaniu wykresu.
   7 dni = 10 081 próbek, 30 dni = ok. 43 000, a serii jest do 6.
   Przy 30 dniach to 259 194 kawałków krzywej na jedno odrysowanie.
   Telefon nie nadążał — dlatego wyglądało to jak „wolne czytanie
   z pamięci", choć dane były już w pamięci.

2) Tryb NORMALIZOWANY (przełącznik) liczył minimum i maksimum całej serii
   OSOBNO DLA KAŻDEJ PRÓBKI. Dla 7 dni to około 100 mln operacji.
   Włączenie tego trybu zawieszało wykres.

3) Kursor (palec przesuwany po wykresie) miał dokładnie ten sam błąd
   co punkt 2 — czyli rysowanie kursora też muliło.

CO ZMIENIONO W 0.31.47
----------------------
1) Jeden koszyk = jeden piksel szerokości wykresu. Z każdego koszyka
   bierzemy MINIMUM i MAKSIMUM, czyli najwyżej 2 punkty na piksel.
   30 dni: 43 200 punktów na serię -> ok. 1 800.
   Wszystkie serie razem: 259 194 -> ok. 10 800 punktów (redukcja 96%).

   WAŻNE: kształt linii, szczyty i dołki ZOSTAJĄ. Minimum i maksimum
   w koszyku to dokładnie te wartości, które były najwyższe i najniższe
   w tym fragmencie — nic nie jest uśredniane ani wygładzane.

2) Tryb NORMALIZOWANY liczy min/max raz na serię, nie raz na próbkę.

3) Kursor: to samo — min/max raz na serię.

4) Bez zmian: przerwa w danych to nadal puste miejsce (bez linii),
   odcinek symulowany nadal jest kreskowany, a przejście
   fizyczne/symulacja nadal łamie linię w tym samym miejscu.

CZEGO NIE ZMIENIONO
-------------------
- Wygląd: żadnych nowych napisów, plakietek, liczników ani pasków.
- Ustawienia → „Bufor offline" — jest.
- Kafelek SERWO w procentach — jest.
- Pamięć historii w telefonie i dociąganie tylko brakujących dni — jest.
- Wszystko z 0.31.33–0.31.46 — jest.

DWA PLIKI ZMIENIONE
-------------------
- app/src/main/java/com/sterownikco/pro/ui/chart/ChartEngine.kt
- app/build.gradle.kts (tylko numer wersji: 31047 / "0.31.47")

JAK ZBUDOWAĆ (jedna komenda)
----------------------------
W folderze projektu (tym z settings.gradle.kts):

  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Skrypt sam znajduje Javę 17–21, sam znajduje Android SDK, buduje APK
i wypisuje komendę do instalacji:

  adb.exe install -r "app\build\outputs\apk\debug\app-debug.apk"

PO CZYM POZNAĆ, ŻE DZIAŁA
-------------------------
1) W ustawieniach telefonu (Aplikacje → Sterownik CO) wersja to 0.31.47.
2) Wejdź w WYKRESY, wybierz 30 DNI. Linia ma się pojawić od razu,
   bez „dorysowywania się" po kawałku.
3) Przeciągnij palcem po wykresie — kropki i pionowa linia kursora
   mają chodzić płynnie, także przy 30 dniach.
4) Kształt wykresu ma być taki sam jak w 0.31.46. Jeśli gdzieś zniknął
   słupek albo szpic — to błąd, zgłoś.

JEŻELI JEST GORZEJ
------------------
Wróć do 0.31.46 (ten sam sposób budowania, poprzedni folder projektu).

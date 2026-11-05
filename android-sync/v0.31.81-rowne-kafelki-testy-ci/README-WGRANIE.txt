Sterownik CO — aplikacja Android 0.31.81 (pełny projekt)
========================================================

Baza: 0.31.75 (wersja z telefonu) + poprawki 0.31.77-0.31.81.
Zmiana dotyczy TYLKO aplikacji. Piec zostaje na 3.57.9.
versionCode 31081 / versionName "0.31.81" (app/build.gradle.kts).

Trzy rzeczy w 0.31.81
---------------------
1. CI uruchamia teraz TESTY JEDNOSTKOWE (`testDebugUnitTest`), nie tylko
   kompilację. Czerwony test zatrzymuje wydanie.
2. Kafelki w jednym rzędzie mają RÓWNĄ WYSOKOŚĆ — dolne krawędzie stoją
   w jednej linii, także gdy podpis zajmuje dwie linie.
3. Paczka ZIP jest pakowana tak jak poprzednie wydania (pola czasu wpisów).


1) CI sprawdza testy, nie tylko to, że się kompiluje
----------------------------------------------------
Do 0.31.80 workflow `.github/workflows/apk-android.yml` miał tylko
`assembleDebug`. Testy z `app/src/test/` (m.in. AlarmCenterTest,
CommandRedactorTest, ServoModeUiTest, ScreenLayoutTest) nie były uruchamiane
w ogóle — zielony CI dowodził, że źródła główne się kompilują.

Teraz krok `testDebugUnitTest` stoi PO `assembleDebug` (APK powstaje nawet
wtedy, gdy padnie test, więc zawsze jest co obejrzeć), a PRZED uploadem
artefaktu i commitem APK — czyli padnięty test realnie blokuje wydanie.
Log testów idzie do `test.log`, a krok „Blad — ostatnie linie jako adnotacja"
pokazuje na GitHubie ostatnie linie `test.log` i `build.log`.


2) Kafelki w jednym rzędzie — równa wysokość (to ZMIANA WYGLĄDU)
----------------------------------------------------------------
Powód: kafel „DO KOŃCA DNIA" ma podpis, który od 0.31.80 brzmi
„bez słońca · ociepla się" i przy 85,3 dp szerokości zawija się do dwóch
linii. Przez to kafel był o ~10 dp wyższy od „SPADEK DZIŚ" i „PANEL −
POWIETRZE", a dolne krawędzie trzech teł nie stały w jednej linii.

Weather.kt, MetricGrid:

  Row(Modifier.fillMaxWidth().height(IntrinsicSize.Min), ...)   // rząd
      Column(Modifier.weight(1f).fillMaxHeight()...)            // kafelek

IntrinsicSize.Min liczy wysokość rzędu z najwyższego kafelka, fillMaxHeight
rozciąga pozostałe do tej samej wysokości. Efekt: wszystkie trzy tła kończą
się w tej samej linii.

Mówię wprost: to kosmetyka, nie naprawa błędu. Nie ma tu żadnej błędnej
liczby — tylko nierówne krawędzie. Sprawdzenie jest jedno: ekran Pogoda,
karta BILANS & ZYSK, trzy kafelki w rzędzie.


3) ZIP pakowany jak poprzednie wydania
--------------------------------------
Paczka 0.31.80 była o 7 408 B mniejsza od 0.31.79 mimo większego diffu.
Przyczyna nie miała nic wspólnego z projektem: 0.31.80 spakowałem z opcją
`zip -X`, która usuwa pola rozszerzone czasu wpisów („UT", 24 B na wpis,
przechowywane dwa razy — w nagłówku lokalnym i w katalogu centralnym).

  wpisy    pliki   katalogi   pola „UT"   rozmiar
  -----------------------------------------------------------------
  0.31.79   196      150        46        4 704 B    1 199 370 B
  0.31.80   196      150        46            0 B    1 191 962 B
  0.31.81   196      150        46        4 704 B    1 202 399 B

Liczba plików jest ta sama we wszystkich wydaniach od 0.31.70 (150 plików,
196 wpisów razem z katalogami); lista nazw w 0.31.80 i 0.31.81 jest
identyczna, różni się tylko zawartość `Weather.kt`, `build.gradle.kts`
i tego README. Uwaga: wcześniejsze notatki mówiły „148 plików" — to była
pomyłka w liczeniu (0.31.69 miało 149, od 0.31.70 jest 150).

Od 0.31.81 pakuję zwykłym `zip -r`, tak jak wydania 0.31.69-0.31.79.


Co było wcześniej (bez zmian w 0.31.81)
---------------------------------------
0.31.77 — usunięte wykrywanie pobrań wody (zgadywanie ze spadku temperatury).
0.31.78 — tytuł karty zawija się do 2 linii z wielokropkiem. Podział
          nagłówka prognozy z tego wydania był pomyłką i został cofnięty;
          sprostowanie: android-sync/v0.31.78-tytuly-bez-ucinania/ERRATA-0.31.78.txt
0.31.79 — „DO KOŃCA DNIA" z jednego źródła (SolarAnalytics.restOfDayDeltaC),
          opis zależy od znaku liczby, nie od pory dnia.
0.31.80 — warunek „jest jeszcze słońce" w jednym miejscu
          (core/Weather.sunLeftToday); restOfDayDeltaC(sunLeft) nie pokaże
          zysku szczytu po zachodzie; krótkie opisy (≤26 znaków);
          arkusz Czujniki: liczba osobno, opis w drugiej linii.


Brama (bez Gradle/JDK)
----------------------
  python3 narzedzia/sprawdz_uklad.py
  79 OK / 0 BŁĄD dla 0.31.81
  kontrola: 0.31.80 -> 5 BŁĄD (m.in. brak IntrinsicSize.Min i fillMaxHeight)
  Nowe sprawdzenia: równa wysokość kafelków i kolejność kroków w CI
  (assembleDebug -> testDebugUnitTest -> upload).


Instalacja
----------
APK z CI ma podpis debug, inny niż poprzednie wydania:
  1. Odinstaluj starą wersję Sterownik CO.
  2. Zainstaluj SterownikCO-0.31.81-debug.apk.
  3. Sprawdź w Ustawieniach numer wersji: 0.31.81.
Bez odinstalowania instalator zgłosi „aplikacja nie została zainstalowana".

Jeśli CI będzie CZERWONE, to znaczy, że padł któryś test jednostkowy —
wtedy APK się nie pojawi. To zamierzone: wydanie ma nie wychodzić
z czerwonym testem.


Pliki
-----
  SterownikCO-Android-v0.31.81-PELNY-PROJEKT.zip   pełne źródła (to drzewo)
  ZMIANY-0.31.80-do-0.31.81.diff                  różnica wobec 0.31.80
  SHA256SUMS.txt                                  sumy kontrolne
  sprawdz_uklad.py                                brama (79 sprawdzeń)

Gotowy plik do telefonu: android-sync/apk/SterownikCO-0.31.81-debug.apk

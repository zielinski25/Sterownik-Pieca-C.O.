Sterownik CO — aplikacja Android 0.31.82 (pełny projekt)
========================================================

Baza: 0.31.75 (wersja z telefonu) + poprawki 0.31.77-0.31.82.
Piec zostaje na 3.57.9.
versionCode 31082 / versionName "0.31.82" (app/build.gradle.kts).

UWAGA — w aplikacji nie zmienia się NIC poza numerem wersji.
To wydanie porządkuje dwie rzeczy w narzędziach:
  1. liczba testów w CI jest teraz FAKTEM z plików XML, nie liczeniem po kodzie,
  2. poprawiona arytmetyka paczek ZIP (policzona od nowa, nie z pamięci).
Jeśli nie chcesz instalować — nie musisz; 0.31.81 działa tak samo.


1) Poprawiona arytmetyka ZIP (moja pomyłka w 0.31.81)
-----------------------------------------------------
W 0.31.81 napisałem, że różnica 7 408 B między 0.31.79 i 0.31.80 to „pola UT
24 B na wpis, przechowywane dwa razy". To było liczone z pamięci i częściowo
błędne. Zmierzone od nowa (parsowanie nagłówków lokalnych i katalogu
centralnego każdego wpisu, nie średnie):

  wpisów  plików  pola extra: lokalne + centralne   treść skomp.   treść nieskomp.   plik ZIP
  -----------------------------------------------------------------------------------------------
  0.31.79    196     150        5 488 B + 4 704 B      1 147 430        2 974 224    1 199 370 B
  0.31.80    196     150              0 (zip -X)       1 150 214        2 976 509    1 191 962 B
  0.31.80*   196     150        5 488 B + 4 704 B      1 150 214        2 976 509    1 202 154 B
  0.31.81    196     150        5 488 B + 4 704 B      1 150 459        2 976 668    1 202 399 B

  * to samo drzewo 0.31.80 spakowane zwykłym `zip -r`, czyli tym, czym pakowałem
    0.31.79 i 0.31.81 — potrzebne, żeby porównywać jabłka z jabłkami.

Co z tego wynika (rachunek domyka się co do bajtu):

  1 199 370 − 10 192 + 2 784 = 1 191 962

  • 10 192 B = wszystkie pola rozszerzone („UT" i towarzyszące):
    5 488 B w nagłówkach lokalnych + 4 704 B w katalogu centralnym.
    Wcześniej pisałem „4 704 B razy dwa" — to było zaniżone, bo nagłówek
    lokalny ma 28 B na wpis, a centralny 24 B.
  • +2 784 B to WZROST treści (skompresowanej) w 0.31.80 wobec 0.31.79;
    nieskompresowanej: +2 285 B. Treść UROSŁA, nie zmalała — kierunek był
    w mojej notatce dobry, ale liczba 2 784 nie była pokazana razem z rachunkiem.

Wniosek bez zmian: w projekcie nic nie zginęło — liczba plików jest ta sama
(150 plików, 196 wpisów z katalogami) we wszystkich wydaniach od 0.31.70,
a lista nazw 0.31.80 / 0.31.81 jest identyczna.


2) Liczba testów z JUnit XML, nie z kodu
-----------------------------------------
W 0.31.81 podałem „162 testy" policzone po `@Test` w źródłach. To nie jest
fakt o uruchomieniu — tyle tylko, ile metod z adnotacją jest w kodzie.

Teraz krok `Liczba testow z JUnit XML` parsuje
`app/build/test-results/testDebugUnitTest/*.xml` i sumuje `tests`, `failures`,
`errors`, `skipped` ze wszystkich `<testsuite>`. Wynik:

  • idzie do podsumowania runu (`$GITHUB_STEP_SUMMARY`),
  • idzie do logu kroku (widoczne w runie),
  • i jest zapisywany do `android-sync/CI-TESTY.txt`, który commit z APK
    zabiera do repo — to jedyny kanał, którym odczytam wynik, bo logi runów
    nie są dla mnie dostępne.

Krok kończy się błędem, gdy `tests == 0` albo gdy jest choć jeden `failure`
lub `error`, więc „zero testów" nie przejdzie jako sukces.


3) Dziura, którą przy okazji zamknąłem: `| tee` maskowało kod wyjścia
---------------------------------------------------------------------
Oba kroki Gradle kończyły się potokiem `./gradlew ... 2>&1 | tee build.log`.
Domyślny shell runnera to `bash -e` BEZ `pipefail`, więc kod wyjścia kroku
brał się z `tee`, a nie z Gradle — nieudany build albo czerwony test mógł
skończyć się zielonym krokiem. W 0.31.81 testy „przeszły", ale to nie
dowodziło jeszcze, że bramka działa.

Teraz oba kroki mają jawnie `set -o pipefail`. Brama pilnuje, żeby każdy
`| tee` w workflow miał nad sobą `set -o pipefail`.


Co było wcześniej (bez zmian w 0.31.82)
---------------------------------------
0.31.77 — usunięte wykrywanie pobrań wody (zgadywanie ze spadku temperatury).
0.31.78 — tytuł karty zawija się do 2 linii z wielokropkiem; podział nagłówka
          prognozy z tego wydania był pomyłką (cofnięty w 0.31.79).
          Sprostowanie: android-sync/v0.31.78-tytuly-bez-ucinania/ERRATA-0.31.78.txt
0.31.79 — „DO KOŃCA DNIA" z jednego źródła (SolarAnalytics.restOfDayDeltaC),
          opis zależny od znaku liczby.
0.31.80 — warunek „jest jeszcze słońce" w jednym miejscu
          (core/Weather.sunLeftToday), restOfDayDeltaC(sunLeft), krótkie opisy,
          arkusz Czujniki: liczba osobno, opis w drugiej linii.
0.31.81 — CI uruchamia testy jednostkowe; równe wysokości kafelków
          (IntrinsicSize.Min + fillMaxHeight).


Brama (bez Gradle/JDK)
----------------------
  python3 narzedzia/sprawdz_uklad.py
  82 OK / 0 BŁĄD dla 0.31.82 (trzy nowe sprawdzenia: JUnit XML, CI-TESTY.txt, pipefail)
  kontrola: 0.31.81 -> 3 BŁĄD (numer wersji i kod wersji)


Instalacja
----------
APK z CI ma podpis debug, inny niż wydania z Twojego komputera:
  1. Odinstaluj starą wersję Sterownik CO.
  2. Zainstaluj SterownikCO-0.31.82-debug.apk.
  3. Sprawdź w Ustawieniach numer wersji: 0.31.82.

Jeśli nie chcesz instalować — nic nie tracisz: 0.31.82 różni się od 0.31.81
tylko numerem wersji. Kod aplikacji jest ten sam.


Pliki
-----
  SterownikCO-Android-v0.31.82-PELNY-PROJEKT.zip   pełne źródła (to drzewo)
  ZMIANY-0.31.81-do-0.31.82.diff                  różnica wobec 0.31.81
  SHA256SUMS.txt                                  sumy kontrolne
  sprawdz_uklad.py                                brama (82 sprawdzenia)

W repo, poza ZIP-em: `android-sync/CI-TESTY.txt` — faktyczna liczba testów
z ostatniego runu. Gotowy plik do telefonu:
android-sync/apk/SterownikCO-0.31.82-debug.apk

Sterownik CO — aplikacja Android 0.31.83 (pełny projekt)
========================================================

Baza: 0.31.75 (wersja z telefonu) + poprawki 0.31.77-0.31.83.
Piec zostaje na 3.57.9.
versionCode 31083 / versionName "0.31.83" (app/build.gradle.kts).

UWAGA — w aplikacji nie zmienia się NIC poza numerem wersji.
To wydanie prostuje moje nieprawdziwe zdanie z 0.31.82 o pipefail, przynosi
dowód z żywego runu, że czerwony test blokuje wydanie, i poprawia bramę.
Jeśli nie chcesz instalować — nie musisz; 0.31.82 działa identycznie.


1) SPROSTOWANIE: runner GitHuba MA pipefail domyślnie
-----------------------------------------------------
W README 0.31.82 napisałem (sekcja 3), że potok `./gradlew ... | tee`
maskował kod wyjścia Gradle, bo „domyślny shell runnera to `bash -e` BEZ
pipefail". To było nieprawdziwe i jestem to winien wprost:

  runner GitHuba uruchamia `run:` jako
  `bash --noprofile --norc -e -o pipefail`
  (actions/runner, ScriptHandlerHelpers.cs) — pipefail jest domyślnie.

Skąd wziąłem błąd: powtarzałem obiegową opinie o `bash -e` z dokumentacji
zakładki „shell”, nie sprawdzając, jaki shell runner bierze, gdy `shell:`
nie jest podane. Nie sprawdziłem tego ani w kodzie runnera, ani doświadczeniem
— a było jedno i drugie do zrobienia.

Co z tego zostaje:
  • `set -o pipefail` w obu krokach ZOSTAJE — nic nie kosztuje, a działa też
    przy `shell: sh` i przy ręcznym uruchomieniu skryptu,
  • skasowałem za to fałszywe uzasadnienie w komentarzach workflow
    („zamknięta dziura") i w bramie,
  • wniosek o 0.31.81: zielony run z testami naprawdę znaczył, że testy
    przeszły — nie był „zielony mimo czerwonego Gradle".

Sprostowanie leży też obok paczki 0.31.82:
  android-sync/v0.31.82-arytmetyka-i-testy-z-xml/ERRATA-PIPEFAIL.txt
(README tamtego wydania zostawiłem bez zmian, bo jest w środku ZIP 0.31.82.)


2) Dowód, że bramka blokuje — próba na żywym runnerze
------------------------------------------------------
Do 0.31.82 nikt nigdy nie widział czerwonego runu z testami. Zielony run
dowodzi, że testy przechodzą, ale nie dowodzi, że ich porażka ZATRZYMUJE
wydanie. Zrobiłem więc próbę:

  commit 4a585a9 „PROBA BRAMKI (tymczasowo)" — w ScreenLayoutTest dopisany
  test `probaBramkiWymuszonyBlad()` z `error("proba bramki 0.31.82")`.

Wynik runu 41942262419 (push, wersja 0.31.82):

  Set up job                 -> success
  Java 17 / Wersja i SDK     -> success
  assembleDebug              -> success
  testDebugUnitTest          -> FAILURE      <- (a) krok czerwony
  Liczba testow z JUnit XML  -> skipped
  Blad - ostatnie linie      -> success      (adnotacja z test.log)
  APK jako artefakt          -> skipped
  APK do repo                -> skipped      <- (b) brak commitu z APK
  RUN                        -> failure

I najważniejsze: po tym runie gałąź stała w miejscu — na origin HEAD to
nadal 4a585a9 (commit z próbą), nie powstał żaden commit „CI: APK 0.31.82",
a android-sync/CI-TESTY.txt został z poprzedniego, zielonego runu.

Próba została cofnięta: plik wrócił do treści z commita a6a2a6a, a drzewo
`android/` jest znowu BAJT W BAJT takie samo jak w paczce ZIP 0.31.82
(sprawdzone przez rozpakowanie ZIP-a i `diff -r`).

Uwaga o `workflow_dispatch`: próbowałem uruchomić run także ręcznie
(`gh workflow run "APK (Android)"`), ale token tego nie może —
HTTP 403 „Resource not accessible by integration". Dowód pochodzi więc
z runu wywołanego pushem; to ta sama ścieżka kodu (te same kroki, ten sam
runner), tylko inne zdarzenie startowe.


3) Brama: pipefail sprawdzany per krok, nie przez liczenie w całym pliku
-----------------------------------------------------------------------
Poprzednie sprawdzenie liczyło wystąpienia „set -o pipefail" i „| tee"
w całym YAML-u, więc wystarczył KOMENTARZ z tym tekstem, żeby przeszło —
nawet gdy krok nie miał polecenia. Teraz brama dzieli workflow na bloki
`- name: ...` i wymaga, żeby `set -o pipefail` było poleceniem w TYM SAMYM
bloku, w którym jest `| tee`. Sprawdzone doświadczalnie: po skasowaniu
polecenia z kroku assembleDebug (komentarz nadal je cytował) brama daje
BŁĄD: „kroki bez pipefail: assembleDebug".


Co było wcześniej (bez zmian w 0.31.83)
---------------------------------------
0.31.77 — usunięte wykrywanie pobrań wody (zgadywanie ze spadku temperatury).
0.31.78 — tytuł karty zawija się do 2 linii z wielokropkiem; podział nagłówka
          prognozy z tego wydania był pomyłką (cofnięty w 0.31.79).
          Sprostowanie: android-sync/v0.31.78-tytuly-bez-ucinania/ERRATA-0.31.78.txt
0.31.79 — „DO KOŃCA DNIA" z jednego źródła (SolarAnalytics.restOfDayDeltaC).
0.31.80 — warunek „jest jeszcze słońce" w jednym miejscu
          (core/Weather.sunLeftToday), restOfDayDeltaC(sunLeft), krótkie opisy,
          arkusz Czujniki: liczba osobno, opis w drugiej linii.
0.31.81 — CI uruchamia testy jednostkowe; równe wysokości kafelków.
0.31.82 — liczba testów z JUnit XML zapisywana do android-sync/CI-TESTY.txt;
          poprawiona arytmetyka paczek ZIP (5 488 B + 4 704 B = 10 192 B,
          treść 0.31.80 wzrosła o 2 784 B).


Brama (bez Gradle/JDK)
----------------------
  python3 narzedzia/sprawdz_uklad.py
  82 OK / 0 BŁĄD dla 0.31.83
  sprawdzenie pipefail działa teraz per krok (próba: usunięcie polecenia
  z kroku assembleDebug daje BŁĄD, mimo że komentarz je cytuje)


Instalacja
----------
APK z CI ma podpis debug, inny niż wydania z Twojego komputera:
  1. Odinstaluj starą wersję Sterownik CO.
  2. Zainstaluj SterownikCO-0.31.83-debug.apk.
  3. Sprawdź w Ustawieniach numer wersji: 0.31.83.

Jeśli nie chcesz instalować — nic nie tracisz: kod aplikacji jest ten sam
co w 0.31.82, różni się tylko numer wersji.


Pliki
-----
  SterownikCO-Android-v0.31.83-PELNY-PROJEKT.zip   pełne źródła (to drzewo)
  ZMIANY-0.31.82-do-0.31.83.diff                  różnica wobec 0.31.82
  SHA256SUMS.txt                                  sumy kontrolne
  sprawdz_uklad.py                                brama (82 sprawdzenia)

Gotowy plik do telefonu: android-sync/apk/SterownikCO-0.31.83-debug.apk

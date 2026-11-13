TESTY KODU [v3.58.4 PANEL-WYKRESY] CENTRALI - NA PC, BEZ PLYTKI
===============================================================

Co to jest
----------
Testy funkcji, ktora liczy punkty wykresu dla panelu LCD
(GET /api/history/panel, historiaPanelZbudujCsv() w main_centrala.cpp).

Testowany jest tekst WYCIETY z ../src/main_centrala.cpp, nie jego kopia:
wytnij_blok.py wyjmuje z pliku centrali blok miedzy znacznikami
"[v3.58.4 PANEL-WYKRESY-BEGIN]" i "-END]" oraz to, z czego ten blok korzysta
(dwie struktury historii, stale, deklaracje buforow) i kompiluje go zwyklym
g++. To jest wiec dokladnie ten kod, ktory trafia na plytke.

Reszty pliku centrali (ok. 49 tys. linii) na PC skompilowac sie nie da -
wymaga bibliotek ESP32 i plikow .h projektu, ktorych tu nie ma.

Uruchomienie (Linux albo WSL; g++ z C++17, python3)
---------------------------------------------------
    cd tests
    ./uruchom_testy.sh                  testy + porownanie wzorcow dla panelu
    SANITIZER=1 ./uruchom_testy.sh      to samo z AddressSanitizer + UBSan
    ./sprawdz_naglowki_esp32.sh         ten sam kod prawdziwymi naglowkami
                                        arduino-esp32 (opis w naglowku skryptu;
                                        dziala tylko w pelnym repozytorium)

Co sprawdzaja (1434 sprawdzenia)
--------------------------------
Dwa rodzaje sprawdzen:
  a) przypadki z wynikiem policzonym recznie - co MA wyjsc,
  b) porownanie z NIEZALEZNYM liczeniem "sekunda po sekundzie": test ma wlasna,
     prosta i powolna wersje tego samego rachunku (kazda sekunda okna osobno)
     i wymaga, zeby tekst odpowiedzi byl identyczny co do znaku. Na 934
     losowych zestawach danych i w wiekszosci przypadkow z punktu a).

Przypadki:
   1  godzina pracy, probka co 60 s: 96 punktow, naglowek, etykiety, 36 kolumn
   2  skok temperatury w polowie godziny
   3  centrala wylaczona na 30 minut: linie "#przerwa" w jednym ciaglym odcinku;
      odstep 5 minut to jeszcze nie przerwa, 7 minut - juz tak
   4  pelny bufor krotki (1440 probek, poczatek w srodku tablicy)
   5  tydzien: bufor dlugi (zdarzenia) + ostatnia doba z bufora krotkiego;
      wartosc zdarzenia obowiazuje do nastepnego wpisu (srednia wazona czasem);
      przerwa w buforze dlugim (ponad 90 minut bez wpisu)
   6  pusta historia, bufory nieprzydzielone, jedna probka, dwie probki
   7  centrala bez zegara; zegar zlapany po 10 minutach; zegar zgubiony
   8  wartosci nietypowe: ujemne, NaN, nieskonczonosc, "-0.0", najdluzsza
      mozliwa linia (289 znakow - bufor linii panelu ma 300)
   9  parametry: pkt poza zakresem, zly tryb (kod 400), fs=0
  10  brak pamieci w centrali: bez PSRAM, brak pamieci na kubelki, na tekst
      odpowiedzi, odmowa w dowolnym z 98 miejsc dopisywania - zawsze kod 503,
      nigdy ucieta odpowiedz z kodem 200; nic nie wycieka
  11  zegar centrali skacze: czas cofniety, probka z roku 2100 i z roku 2000
  12  dane losowe (934 zestawy)
  13  oba bufory pelne (8000 zdarzen + 1440 probek): rozmiar odpowiedzi

Przypadek 1, przypadki z brakiem pamieci (10) i kazdy z losowych zestawow (12)
sprawdzaja tez: pamiec zwolniona, spinlock oddany, zadnego przydzialu pamieci
pod spinlockiem. Z SANITIZER=1 calosc biegnie dodatkowo pod AddressSanitizer
i UBSan (wyjscie poza tablice, wyciek, przepelnienie liczby = blad testu).

Wzorce dla testow panelu LCD (krok [4] skryptu)
------------------------------------------------
Ten sam program wypisuje 9 odpowiedzi centrali do plikow i porownuje je z
LCD-Centrali/testy_host/wzorce_centrali/ (to samo repozytorium). Na tych
plikach testowany jest firmware panelu. Roznica = centrala odpowiada teraz
inaczej, niz zaklada test panelu; skrypt konczy sie wtedy bledem.
Po SWIADOMEJ zmianie formatu:
    AKTUALIZUJ_WZORCE=1 ./uruchom_testy.sh
i ponownie testy panelu (LCD-Centrali/testy_host/uruchom_testy.sh).
Gdy paczka centrali jest rozpakowana osobno (katalogu panelu nie ma), ten
krok jest pomijany.

Czego te testy NIE sprawdzaja
-----------------------------
- Kompilacji calego main_centrala.cpp dla ESP32 (PlatformIO). Zrobi to dopiero
  Twoj build.
- Rejestracji trasy HTTP (wifiWebServer.on(...) - 9 linii kodu). Uzywa tych
  samych wywolan co sasiednie trasy /api/history i /api/history/long.
- Dzialania na plytce: czasu liczenia (na PC pelne bufory to ok. 3 ms; na
  ESP32-S3 nie mierzone), zachowania przy zapisie nowej probki w trakcie
  liczenia (kod kopiuje kazdy wiersz i przycina czasy do okna, ale wyscigu
  dwoch rdzeni PC nie odtwarza), prawdziwego braku pamieci.
- Atrapy String / ps_malloc / spinlocka / DLOGF sa w test_panel_hist.cpp.
  Zgodnosc z prawdziwymi naglowkami arduino-esp32 sprawdza osobno
  sprawdz_naglowki_esp32.sh (tylko skladnia i typy, bez linkowania).

Pliki
-----
  wytnij_blok.py               wycina kod z main_centrala.cpp
  test_panel_hist.cpp          atrapy + testy
  uruchom_testy.sh             buduje, uruchamia, porownuje wzorce
  sprawdz_naglowki_esp32.sh    sprawdzenie prawdziwymi naglowkami ESP32

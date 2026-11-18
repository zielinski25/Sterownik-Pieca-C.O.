TESTY KODU [v3.58.4 PANEL-WYKRESY] CENTRALI (STAN Z v3.58.5) - NA PC, BEZ PLYTKI
================================================================================

Co to jest
----------
Testy funkcji, ktora liczy punkty wykresu dla panelu LCD
(GET /api/history/panel, historiaPanelZbudujCsv() w main_centrala.cpp).

Testowany jest tekst WYCIETY z ../src/main_centrala.cpp, nie jego kopia:
wytnij_blok.py wyjmuje z pliku centrali blok miedzy znacznikami
"[v3.58.4 PANEL-WYKRESY-BEGIN]" i "-END]" oraz to, z czego ten blok korzysta
(dwie struktury historii, stale, deklaracje buforow, piec pol zegara
aktualnaData* / aktualnyCzas*) i kompiluje go zwyklym g++. To jest wiec
dokladnie ten kod, ktory trafia na plytke.

Reszty pliku centrali (ok. 49 tys. linii) na PC skompilowac sie nie da -
wymaga bibliotek ESP32 i plikow .h projektu, ktorych tu nie ma.

Uruchomienie (Linux albo WSL; g++ z C++17, python3)
---------------------------------------------------
    cd tests
    ./uruchom_testy.sh                  testy + porownanie wzorcow dla panelu
    SANITIZER=1 ./uruchom_testy.sh      to samo z AddressSanitizer + UBSan
    ./porownaj_z_3.58.4.sh              co zmienia 3.58.5: tabela "po restarcie
                                        centrali" dla obu wersji i porownanie
                                        odpowiedzi na losowych danych
    python3 sprawdz_czulosc.py          czy testy wykrywaja celowo wprowadzone
                                        bledy (ok. 2 minut)
    ./sprawdz_naglowki_esp32.sh         ten sam kod prawdziwymi naglowkami
                                        arduino-esp32 (opis w naglowku skryptu;
                                        dziala tylko w pelnym repozytorium)

Co sprawdzaja (2697 sprawdzen)
------------------------------
Dwa rodzaje sprawdzen:
  a) przypadki z wynikiem policzonym recznie - co MA wyjsc,
  b) porownanie z NIEZALEZNYM liczeniem "sekunda po sekundzie": test ma wlasna,
     prosta i powolna wersje tego samego rachunku (kazda sekunda okna osobno,
     takze wybor okna) i wymaga, zeby tekst odpowiedzi byl identyczny co do
     znaku. Na 1834 losowych zestawach danych i w wiekszosci przypadkow
     z punktu a).

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
Nowe w v3.58.5:
  14  po restarcie centrali: stara historia z plikow + nowe pomiary.
      Stara historia sprzed 40 dni, 3 dni, 5 godzin; 1, 2 i 30 pomiarow po
      restarcie; w kazdym trybie okno zaczyna sie od pierwszej sekundy zakresu
      z pomiarem; granica zakresu co do sekundy
  15  zegar centrali: odczyt biezacego czasu z pol aktualnaData*/aktualnyCzas*
      (czas zimowy i letni, pola wyzerowane, wartosci spoza zakresu); prog
      6 minut dla bufora krotkiego i 90 minut dla dlugiego co do minuty;
      historia konczaca sie w przeszlosci -> okno do biezacego czasu, na koncu
      przerwa; sama stara historia -> 96 przerw; zegar wczesniejszy niz
      probka albo nieznany -> bez wplywu; probki z czasem umownym -> bez wplywu
  16  dane losowe: stara historia + odstep + nowe pomiary + zegar centrali
      (900 zestawow)

Przypadek 1, przypadki z brakiem pamieci (10) i kazdy z losowych zestawow
(12, 16) sprawdzaja tez: pamiec zwolniona, spinlock oddany, zadnego przydzialu
pamieci pod spinlockiem. Z SANITIZER=1 calosc biegnie dodatkowo pod
AddressSanitizer i UBSan (wyjscie poza tablice, wyciek, przepelnienie liczby
= blad testu).

Porownanie z 3.58.4 (porownaj_z_3.58.4.sh)
------------------------------------------
Ten sam test_panel_hist.cpp kompilowany jest z kodem wycietym z centrali
3.58.4 i z 3.58.5 (tryby --po-restarcie i --skroty; zwykle testy sa pisane
pod 3.58.5 i na kodzie 3.58.4 nie przechodza - tak ma byc). Skrypt wypisuje:
  - co kazda wersja odpowiada panelowi w 5 sytuacjach po restarcie centrali,
    po 0, 1, 2, 10, 30 i 120 minutach, i jak dlugo panel pokazuje napis
    zamiast wykresu,
  - dla 1834 losowych zestawow: czy odpowiedzi obu wersji sa identyczne.
    O tym, czy MAJA byc identyczne, decyduje osobny rachunek w tescie
    (refBezZmianWzgledem3584), nie kod centrali. Zestaw, ktory mial byc bez
    zmian, a sie rozni, konczy skrypt bledem.
Wynik z dnia wydania: ../WYNIKI/po-restarcie-przed-i-po.txt.
Plik centrali 3.58.4: z katalogu ../../v3.58.4-panel-wykresy/ (pelne
repozytorium), ze zmiennej STARY_CPP albo odtworzony z latki
../main_centrala-v3.58.4-do-v3.58.5.patch (wymaga programu "patch").

Czulosc testow (sprawdz_czulosc.py)
-----------------------------------
14 razy psuje jedna rzecz w kodzie dodanym w v3.58.5 (na kopii) i sprawdza,
ze testy to wykrywaja. Wynik z dnia wydania: ../WYNIKI/testy-czulosc.txt.

Wzorce dla testow panelu LCD (krok [4] skryptu uruchom_testy.sh)
----------------------------------------------------------------
Ten sam program wypisuje 11 odpowiedzi centrali do plikow i porownuje je z
LCD-Centrali/testy_host/wzorce_centrali/ (to samo repozytorium). Na tych
plikach testowany jest firmware panelu. Roznica = centrala odpowiada teraz
inaczej, niz zaklada test panelu; skrypt konczy sie wtedy bledem. Bledem jest
tez plik wzorca w katalogu panelu, ktorego ten kod nie wypisuje.
9 wzorcow jest co do bajta takich samych jak z centrali 3.58.4. Dwa nowe
(godz_po_restarcie_pusto.txt, tydz_po_restarcie.txt) to odpowiedzi centrali
3.58.5 tuz po restarcie z wylaczonym zapisem historii.
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
- Dzialania na plytce: czasu liczenia (na PC pelne bufory to 2-3 ms; na
  ESP32-S3 nie mierzone), zachowania przy zapisie nowej probki w trakcie
  liczenia (kod czyta kazdy wiersz osobno i przycina czasy do okna, ale
  wyscigu dwoch rdzeni PC nie odtwarza), prawdziwego braku pamieci.
- Prawdziwego zegara centrali. W tescie pola aktualnaData*/aktualnyCzas*
  ustawia test; na plytce ustawia je RTC() co 2 s. Zmiana pol w trakcie
  odczytu (drugi rdzen) nie jest odtwarzana - kod czyta je dwa razy i przy
  roznicy powtarza odczyt, co sprawdzone jest tylko przegladem kodu.
- Atrapy String / ps_malloc / spinlocka / DLOGF sa w test_panel_hist.cpp.
  Zgodnosc z prawdziwymi naglowkami arduino-esp32 sprawdza osobno
  sprawdz_naglowki_esp32.sh (tylko skladnia i typy, bez linkowania).

Pliki
-----
  wytnij_blok.py               wycina kod z main_centrala.cpp
  test_panel_hist.cpp          atrapy + testy
  uruchom_testy.sh             buduje, uruchamia, porownuje wzorce
  porownaj_z_3.58.4.sh         3.58.4 i 3.58.5 obok siebie
  sprawdz_czulosc.py           celowo wprowadzone bledy
  sprawdz_naglowki_esp32.sh    sprawdzenie prawdziwymi naglowkami ESP32

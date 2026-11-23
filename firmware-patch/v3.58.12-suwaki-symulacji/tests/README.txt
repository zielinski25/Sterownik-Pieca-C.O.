TESTY PACZKI 3.58.12 (na PC, bez plytki)
========================================
./uruchom_testy.sh               czesci A-C
CPP_PATH=/inny/main_centrala.cpp  - sprawdza wskazany plik
JSDOM_DIR=/katalog                - gdzie jest/ma byc zainstalowany pakiet jsdom (npm i jsdom@24)

A  test_suwaki_symulacji.js <plik .cpp albo .html>
   Wycina funkcje symulacjaSekcjaHtml() z HTML (z raw-stringu R"PIECHTML( w pliku
   centrali albo wprost z Piec.html) i uruchamia ja w jsdom z atrapami
   (S, val(), symAktywna(), sendCommand(), live$/refreshSheet() jak na stronie).
   Scenariusz z telefonu, 13 sprawdzen: symulacja aktywna 21,0 C / 60 min;
   pointerdown + input na suwaku BEZ fokusu -> 45,0; 5 x refreshSheet() -> suwak
   dalej 45,0; nieruszany "Czas trwania" podaza za sterownikiem (59 min); ruszony
   (30) nie jest cofany; Zastosuj wysyla "symuluj bojler 45.0 30"; po Zastosuj
   suwaki znow sledza sterownik (50,0 z Telegrama widoczne); Wylacz teraz;
   nowa symulacja z zewnatrz po wylaczeniu pokazuje swoje parametry.
   A2: to samo na Piec.html; A3: 3.58.11 ma NIE zaliczyc (5 BLAD) - dowod, ze
   test wykrywa wade.
B  FIRMWARE_VERSION, pojedyncza deklaracja symEdycja, znaczniki [v3.58.12],
   node --check obu blokow <script>, 3.58.11 + patch = ten plik (SHA).
C  testy DYM-SYM z paczki 3.58.11 (czesc A tamtego skryptu, 21 sprawdzen) na tym
   pliku z numerem wersji podmienionym na 3.58.11 (tamten skrypt go wymaga).

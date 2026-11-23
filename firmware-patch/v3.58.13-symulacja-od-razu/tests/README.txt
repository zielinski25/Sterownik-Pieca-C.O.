TESTY PACZKI 3.58.13 (na PC, bez plytki)
========================================
./uruchom_testy.sh               czesci A-D
CPP_PATH=/inny/main_centrala.cpp  - sprawdza wskazany plik
JSDOM_DIR=/katalog                - gdzie jest/ma byc zainstalowany pakiet jsdom (npm i jsdom@24)

A  test_suwaki_symulacji.js <plik .cpp albo .html>
   Wycina funkcje symulacjaSekcjaHtml() z HTML (z raw-stringu R"PIECHTML( w pliku
   centrali albo wprost z Piec.html) i uruchamia ja w jsdom z atrapami
   (S, val(), symAktywna(), sendCommand(), live$/refreshSheet() jak na stronie).
   18 sprawdzen: 13 z 3.58.12 (suwak bez fokusu nie jest cofany, Zastosuj wysyla
   "symuluj bojler 45.0 30", Wylacz teraz, nowa symulacja z zewnatrz) + 5 nowych:
   po potwierdzonym Zastosuj status sterownika niesie jeszcze stary odczyt (jak
   firmware <= 3.58.12 przez 60-120 s) -> suwaki zostaja na zadanych 45.0 / 30 min;
   gdy sterownik zglosi 45.0 -> znow podazaja; firmware z odczytem od razu
   (atrapa odczytOdRazu=true) -> blokada zwalnia sie natychmiast i "Czas trwania"
   odlicza; zmiana z Telegrama widoczna.
   A2: to samo na Piec.html; A3: 3.58.12 ma NIE zaliczyc (1 BLAD) - dowod, ze
   test wykrywa wade.
B  test_sym_od_razu.cpp + wytnij_sym_od_razu.py
   Wycina z pliku funkcje symulacjaUstawOdczytOdRazu() i kompiluje ja g++ -Wall
   -Wextra -Werror z atrapa String i 9 zmiennymi odczytu; 11 sprawdzen (kazde pole
   -> tylko swoja zmienna; "dym"/nieznane -> nic).
C  FIRMWARE_VERSION, helper zdefiniowany raz i przed oboma wywolaniami (komenda
   "symuluj" + /api/symulacja-czujnik), zmienne zadeklarowane przed helperem,
   node --check obu blokow <script>, 3.58.12 + patch = ten plik (SHA).
D  testy DYM-SYM z paczki 3.58.11 (21 sprawdzen) na tym pliku z numerem wersji
   podmienionym na 3.58.11 (tamten skrypt go wymaga).

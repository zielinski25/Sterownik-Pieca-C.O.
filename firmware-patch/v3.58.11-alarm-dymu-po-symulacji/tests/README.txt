TESTY PACZKI 3.58.11 (na PC, bez plytki)
========================================
./uruchom_testy.sh              czesci A-D (g++); SANITIZER=1 dodaje ASan+UBSan
./sprawdz_naglowki_esp32.sh     wycinki skompilowane prawdziwymi naglowkami ESP32
                                (RDZEN=3.3.8 OSTRZEZENIA=1; potrzebuje pelnego repo
                                i LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh,
                                ktory pobiera naglowki z GitHub)
CPP_PATH=/inny/main_centrala.cpp  - oba skrypty sprawdza wskazany plik

A  wytnij_dym_sym.py + test_dym_sym.cpp
   blok // --- DYM-SYM-POCZATEK/KONIEC --- i PRAWDZIWY fragment pozar()
   (od "if (czujnikDymuSwiezyOdczyt && czujnikDymuOdczyt > progAlarmowyDymu)"
   do konca galezi dezaktywacji) wyciete z pliku centrali; atrapy: millis(),
   noTone/digitalWrite, log, serwa, rozpalanie. 6 scenariuszy, 21 sprawdzen:
   [1] alarm z symulacji gasnie po "Wylacz teraz" bez czekania na swiezy odczyt
       (log powod=koniec_symulacji, buzzer, wyciszenie, nadzor po skasowaniu),
   [2] alarm z prawdziwego czujnika NIE gasnie po symulacji; 480 trwa, 469 gasi,
   [3] koniec symulacji + prawdziwe 700 ADC w tym samym ticku -> alarm od razu
       wraca jako prawdziwy i kolejna symulacja go nie kasuje,
   [4] symulacja ponizej progu - nic, [5] wygasniecie symulacji = koniec,
   [6] bez alarmu funkcja zwraca false, koniec liczony raz.
B  znaczniki 3.58.11 w pliku, brak starych fragmentow, sprzatanie po alarmie
   w pozar() tylko przez dymAlarmSkasuj(); patch nalozony na 3.58.10 z repo i
   skrypt zrob_35811.py -> ten sam SHA co w SHA256SUMS.txt.
C  ../../../LCD-Centrali/testy_host/sprawdz_pola_statusu.py - pola /api/status
   a pola czytane przez panel LCD.
D  testy paczek 3.58.10 (ZEW2-FILTR 66, wilgotnosc() 47) i 3.58.7/3.58.6
   (START-TG, OTA panelu, wykresy) uruchomione na pliku 3.58.11 (kopia z numerem
   3.58.10 dla skryptow wycinajacych, ktore sprawdzaja numer wersji).

wytnij_dym_sym_esp32.py     samodzielny .cpp (Arduino.h + atrapy) do sprawdzenia
   prawdziwymi naglowkami ESP32.
zrob_35811.py               buduje plik 3.58.11 z pliku 3.58.10 (kazda podmiana
   dokladnie raz, inaczej stop). Czesc B uruchamia go i porownuje wynik z src/
   i z patchem.

Pliki posrednie: /tmp/centrala_35811_test/ (albo $TMPDIR).

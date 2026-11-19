TESTY PACZKI 3.58.10 (na PC, bez plytki)
========================================
./uruchom_testy.sh              czesci A-E (g++); SANITIZER=1 dodaje ASan+UBSan
./sprawdz_naglowki_esp32.sh     wycinki skompilowane prawdziwymi naglowkami ESP32
                                (RDZEN=3.3.8 OSTRZEZENIA=1; potrzebuje pelnego repo
                                i LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh,
                                ktory pobiera naglowki z GitHub)
CPP_PATH=/inny/main_centrala.cpp  - oba skrypty sprawdza wskazany plik

A  wytnij_zew2_filtr.py + test_zew2_filtr.cpp
   blok // --- ZEW2-FILTR-POCZATEK/KONIEC --- wyciety z pliku centrali,
   13 scenariuszy, 66 sprawdzen (czysta logika filtra; [13] = ponowienie po bledzie z 3.58.10).
B  wytnij_wilgotnosc.py + test_wilgotnosc.cpp
   blok Zew 2 (zapiszTempDiag/czytajTempDiag/jsonTempDiag + filtr), zmienne
   dht* i CALA funkcja wilgotnosc() z pliku centrali; atrapy: millis(),
   kolejka odpowiedzi read_dht(), telemetria, DLOGF, String, symulacjaTrwa().
   9 scenariuszy, 47 sprawdzen, liczy odczyty DHT (powtorka po 3 s, ponowienie
   po bledzie po ~6 s max 2, cykl 120 s, 255 s zaniku, symulacja, przepelnienie millis).
C  znaczniki obu linii (3.57.1x i 3.58.x) oraz 3.58.9/3.58.10 w pliku; brak
   starych fragmentow; patch nalozony na 3.58.9 z repo i skrypt zrob_35810.py
   -> ten sam SHA co w SHA256SUMS.txt.
D  ../../../LCD-Centrali/testy_host/sprawdz_pola_statusu.py - pola /api/status
   a pola czytane przez panel LCD.
E  testy paczek 3.58.7 (START-TG) i 3.58.6 (OTA panelu, wykresy) uruchomione
   na pliku 3.58.10 (CPP_PATH=).

wytnij_wilgotnosc_esp32.py  samodzielny .cpp (Arduino.h + atrapy read_dht,
   telemetrii, DLOGF) do sprawdzenia prawdziwymi naglowkami ESP32.
wytnij_start_tg.py          kopia z paczki 3.58.9 (numer wersji 3.58.10).
zrob_35810.py               buduje plik 3.58.10 z pliku 3.58.9 (kazda podmiana
   dokladnie raz, inaczej stop): naglowek, historia, FIRMWARE_VERSION,
   TEMP_DIAG_WYGASZANIE_S, pola i funkcje ponowienia w bloku ZEW2-FILTR,
   bramka/planowanie/logi w wilgotnosc(), log startowy. Czesc C2 uruchamia go
   i porownuje wynik z src/ i z patchem.

Pliki posrednie: /tmp/centrala_35810_test/ (albo $TMPDIR).

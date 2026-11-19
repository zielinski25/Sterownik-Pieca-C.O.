TESTY PACZKI 3.58.9 (na PC, bez plytki)
=======================================
./uruchom_testy.sh              czesci A-E (g++); SANITIZER=1 dodaje ASan+UBSan
./sprawdz_naglowki_esp32.sh     wycinki skompilowane prawdziwymi naglowkami ESP32
                                (RDZEN=3.3.8 OSTRZEZENIA=1; potrzebuje pelnego repo
                                i LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh,
                                ktory pobiera naglowki z GitHub)
CPP_PATH=/inny/main_centrala.cpp  - oba skrypty sprawdza wskazany plik

A  wytnij_zew2_filtr.py + test_zew2_filtr.cpp
   blok // --- ZEW2-FILTR-POCZATEK/KONIEC --- wyciety z pliku centrali,
   12 scenariuszy, 57 sprawdzen (czysta logika filtra).
B  wytnij_wilgotnosc.py + test_wilgotnosc.cpp
   blok Zew 2 (zapiszTempDiag/czytajTempDiag/jsonTempDiag + filtr), zmienne
   dht* i CALA funkcja wilgotnosc() z pliku centrali; atrapy: millis(),
   kolejka odpowiedzi read_dht(), telemetria, DLOGF, String, symulacjaTrwa().
   8 scenariuszy, 33 sprawdzenia, liczy odczyty DHT (powtorka po 3 s, cykl
   120 s, 135 s zaniku, symulacja, przepelnienie millis).
C  znaczniki obu linii (3.57.1x i 3.58.x) w pliku; brak znacznikow konfliktu;
   oba patche nakladane na podstawy z repo (3.58.8, 3.57.13) -> ten sam SHA
   co w SHA256SUMS.txt.
D  ../../../LCD-Centrali/testy_host/sprawdz_pola_statusu.py - pola /api/status
   a pola czytane przez panel LCD.
E  testy paczek 3.58.7 (START-TG) i 3.58.6 (OTA panelu, wykresy) uruchomione
   na pliku 3.58.9 (CPP_PATH=).

wytnij_wilgotnosc_esp32.py  samodzielny .cpp (Arduino.h + atrapy read_dht,
   telemetrii, DLOGF) do sprawdzenia prawdziwymi naglowkami ESP32.
wytnij_start_tg.py          kopia z paczki 3.58.8 (numer wersji 3.58.9).
zrob_3589.py                skrypt, ktorym z wyniku git merge-file (3.57.13 /
   3.57.9 / 3.58.8) powstal plik 3.58.9: rozwiazanie 2 konfliktow, naglowek,
   wpis w historii, FIRMWARE_VERSION, blok filtra, zmiany w wilgotnosc().
   Dokumentuje sposob powstania pliku; nie trzeba go uruchamiac.

Pliki posrednie: /tmp/centrala_3589_test/ (albo $TMPDIR).

TESTY FIRMWARE'U CENTRALI 3.58.8 - NA PC, BEZ PLYTKI
====================================================

  D. [v3.58.8 FW-W-STATUSIE] (nowe): czy /api/status centrali
     (wifiZbudujJsonStatus() + panelOtaKlejJson()) wystawia KAZDE pole,
     ktore panel LCD czyta (diag::POLA_STATUSU_CZYTANE[] + CZUJNIKI_SYMULOWANE[]
     w LCD-Centrali/src/diagnostyka_panelu.h). Skrypt:
     LCD-Centrali/testy_host/sprawdz_pola_statusu.py - czyta prawdziwy plik
     centrali, nie atrape. uruchom_testy.sh puszcza go na tym pliku (ma
     przejsc) i na pliku 3.58.7 (ma zglosic brak "fw" - inaczej sprawdzenie
     nic nie sprawdza). [D3]: linia "fw" jest jedna i stoi po "rssi".
  C. [v3.58.7 START-TG], A. [v3.58.6 PANEL-OTA], B. [v3.58.4 PANEL-WYKRESY]:
     testy z poprzednich paczek na tym pliku - kod bez zmian.

Uruchomienie (Linux albo WSL; g++ C++17, python3; pelne repozytorium):
    cd tests
    ./uruchom_testy.sh                  D + C (40) + A (166) + B (2697)
    SANITIZER=1 ./uruchom_testy.sh      z AddressSanitizer + UBSan
    ./sprawdz_naglowki_esp32.sh         nowa linia (Arduino.h: String, F())
                                        i blok START-TG prawdziwymi naglowkami
                                        arduino-esp32; potem A i B skryptem 3.58.6
    RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh
    OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh

Pliki:
    uruchom_testy.sh, sprawdz_naglowki_esp32.sh
    wytnij_fw_status.py       wycina linie "fw" z wifiZbudujJsonStatus() do
                              szkieletu funkcji ze String/F() (sprawdzenie
                              naglowkami ESP32)
    wytnij_start_tg.py        kopia z paczki 3.58.7 z numerem wersji 3.58.8
Wyniki z dnia wydania: ../WYNIKI.

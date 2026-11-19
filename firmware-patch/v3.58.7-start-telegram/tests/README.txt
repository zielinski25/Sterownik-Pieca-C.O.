TESTY FIRMWARE'U CENTRALI 3.58.7 - NA PC, BEZ PLYTKI
====================================================

Co to jest
----------
Testy na tekscie WYCIETYM z ../src/main_centrala.cpp (nie na kopii):

  C. [v3.58.7 START-TG] - nowe w tej wersji: decyzja, czy po starcie wyslac
     wiadomosc na Telegram, jej tresc, rodzaj niezgodnosci TAGCHECK i tresci
     ostrzezen. Blok miedzy znacznikami "[v3.58.7 START-TG-BEGIN]" i "-END]"
     nie uzywa String ani Arduino - kompiluje sie zwyklym g++ z jedna atrapa
     (enum esp_reset_reason_t, lista jak w ESP-IDF).
  A. [v3.58.6 PANEL-OTA] i B. [v3.58.4 PANEL-WYKRESY] - kod bez zmian
     wzgledem 3.58.6; uruchamiane sa testy z paczki 3.58.6
     (../../v3.58.6-panel-ota/tests) na pliku 3.58.7. Dodatkowo blok
     PANEL-OTA jest porownywany bajt w bajt z kopia, ktorej uzywaja testy
     panelu LCD (LCD-Centrali/testy_host/kod_centrali/panel_ota_blok.inc).

Reszty pliku centrali (ok. 50 tys. linii) na PC skompilowac sie nie da -
wymaga bibliotek ESP32 i plikow .h projektu, ktorych tu nie ma.

Uruchomienie (Linux albo WSL; g++ z C++17, python3)
---------------------------------------------------
    cd tests
    ./uruchom_testy.sh                  C (40 sprawdzen) + A (166) + B (2697)
    SANITIZER=1 ./uruchom_testy.sh      to samo z AddressSanitizer + UBSan
    ./sprawdz_naglowki_esp32.sh         C (blok + kod klejacy + fragment
                                        TAGCHECK) prawdziwymi naglowkami
                                        arduino-esp32 (Arduino.h,
                                        Preferences.h, esp_system.h,
                                        esp_ota_ops.h), potem A i B skryptem
                                        z 3.58.6. Tylko w pelnym repozytorium
                                        (korzysta ze skryptu panelu LCD;
                                        pierwsze uruchomienie pobiera
                                        naglowki z GitHuba, ok. 250 MB).
    RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh      rdzen 3.3.8 (ten ma centrala)
    OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh    liczy tez ostrzezenia clang

Gdy paczka jest rozpakowana osobno (bez katalogu v3.58.6-panel-ota obok),
uruchom_testy.sh robi tylko czesc C, a sprawdz_naglowki_esp32.sh konczy sie
z komunikatem.

Pliki
-----
    uruchom_testy.sh            wycina blok (python w srodku), kompiluje,
                                uruchamia; potem testy 3.58.6 i porownanie
    test_start_telegram.cpp     40 sprawdzen
    sprawdz_naglowki_esp32.sh   prawdziwe naglowki ESP32
    wytnij_start_tg.py          wycinek dla sprawdz_naglowki_esp32.sh: blok,
                                kod klejacy wyslijWiadomoscStartowa(),
                                fragment TAGCHECK, OTA_CONFIRM_UPTIME_MS ze
                                static_assert; deklaracje otoczenia sa
                                sprawdzane po tekscie w pliku centrali

Wyniki z dnia wydania: katalog ../WYNIKI.

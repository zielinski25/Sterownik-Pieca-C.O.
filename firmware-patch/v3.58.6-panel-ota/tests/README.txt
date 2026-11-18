TESTY FIRMWARE'U CENTRALI 3.58.6 - NA PC, BEZ PLYTKI
====================================================

Co to jest
----------
Dwie grupy testow, obie na tekscie WYCIETYM z ../src/main_centrala.cpp, a nie
na jego kopii:

  A. [v3.58.6 PANEL-OTA] - posrednictwo aktualizacji panelu LCD (nowe w tej
     wersji): decyzje, stan i kod klejacy (log, NVS, Telegram, odpowiedzi HTTP).
  B. [v3.58.4 PANEL-WYKRESY] - punkty wykresu dla panelu LCD. Kod bez zmian
     wzgledem 3.58.5; testy te same co w paczce 3.58.5.

wytnij_blok.py wyjmuje z pliku centrali fragmenty miedzy znacznikami
(np. "[v3.58.6 PANEL-OTA-BEGIN]" i "-END]") i to, z czego korzystaja. To jest
wiec dokladnie ten kod, ktory trafia na plytke.

Reszty pliku centrali (ok. 50 tys. linii) na PC skompilowac sie nie da -
wymaga bibliotek ESP32 i plikow .h projektu, ktorych tu nie ma.

Uruchomienie (Linux albo WSL; g++ z C++17, python3)
---------------------------------------------------
    cd tests
    ./uruchom_testy.sh                  A (166 sprawdzen) + B (2697 sprawdzen)
    SANITIZER=1 ./uruchom_testy.sh      to samo z AddressSanitizer + UBSan
    python3 sprawdz_czulosc_ota.py      A: czy testy wykrywaja 43 celowo
                                        wprowadzone bledy (ok. 2 minut)
    python3 sprawdz_czulosc.py          B: to samo, 14 bledow (ok. 2 minut)
    ./sprawdz_naglowki_esp32.sh         A i B prawdziwymi naglowkami
                                        arduino-esp32 i ESPAsyncWebServer
                                        (opis w naglowku skryptu; dziala tylko
                                        w pelnym repozytorium, pobiera naglowki
                                        z GitHuba)
    ./sprawdz_prawdziwe_obrazy.sh       A: prawdziwe pliki .bin (z repozytorium
                                        esptool) przez kod, ktory ocenia plik
                                        z wydania; wymaga sieci i curl

Wyniki z dnia wydania: katalog ../WYNIKI.


A. POSREDNICTWO AKTUALIZACJI PANELU (test_panel_ota.cpp, 166 sprawdzen)
-----------------------------------------------------------------------
Kod jest w trzech fragmentach:
  [v3.58.6 PANEL-OTA-BEGIN/END]   decyzje. Czysty kod: bez klasy String, bez
                                  przydzialow pamieci, bez Arduino.
  [v3.58.6 PANEL-OTA-STAN-...]    zmienne stanu i sekcja krytyczna.
  [v3.58.6 PANEL-OTA-KLEJ-...]    skutki decyzji: log, NVS, Telegram, HTTP.

Test podstawia otoczenie (String, sekcja krytyczna, sterta, NVS, log,
Telegram, zapytanie HTTP, obecnosc pliku, alarmy, dwie funkcje GitHuba)
i prowadzi przez ten kod cale przebiegi - tak, jak wola go firmware.

Grupy sprawdzen:
  [1] identyfikator pliku z sumy SHA-256; sprawdzenie naglowka pliku (program
      ESP32, uklad ESP32, suma na koncu); czytanie odmowy panelu
      "<nr>-<powod>" i powodu czekania - takze teksty za dlugie i ze znakami
      specjalnymi
  [2] decyzje: potwierdzenie po identyfikatorze pliku (sama wersja rowna
      tagowi nie wystarcza panelowi v1.4.0), odmowa z numerem oferty (odmowa
      starej oferty nie konczy nowej), starszy panel, kody 404 / 503 / 409,
      limit 4 pobran, licznik polaczen, limit 24 h, licznik czasu centrali po
      przekreceniu
  [3] tresc kazdej wiadomosci; najdluzsza ma 350 znakow (bufor: 480)
  [4] panel v1.4.0: /update_panel -> pobranie -> start z nowego pliku ->
      potwierdzenie; wpisy w logu, klucze NVS, haslo, odmowa /update_panel
      w trakcie wysylania pliku; obsluga HTTP niczego nie wysyla i nie pisze
      do NVS
  [5] panel do v1.3.0: jedna proba, potem koniec oferty (bez petli)
  [6] odmowa panelu, alarm (oferta ukryta, plik: 503), limit czasu,
      anulowanie, jedno przypomnienie z wlasciwa przyczyna
  [7] /update_panel: GitHub nie odpowiada, pobieranie nieudane, w wydaniu
      firmware centrali zamiast panelu, panel pracuje juz na tym pliku,
      zastapienie czekajacej oferty, numer oferty po 999999999; wynik
      poprzedniej oferty przychodzi od panelu w chwili, gdy komenda pyta
      GitHuba o nastepna (kolejnosc wiadomosci, znacznik NVS nowej oferty)
  [8] restart centrali z czekajaca oferta: oferta wraca; zapis sprzed 3.58.6
      albo brak pliku - nie wraca, jest wiadomosc
  [9] pamiec robocza zadania Telegrama (800 B ze sterty zamiast stosu): bez
      oferty obieg jej nie bierze; kazdy blok jest zwalniany; gdy sterta
      odmowi - wynik aktualizacji, limit 24 h, przypomnienie i anulowanie nie
      przepadaja, tylko czekaja na nastepny obieg albo ponowna komende
  [10] zasady zebrane ze wszystkich przebiegow: w sekcji krytycznej nie ma
      String, sterty, logu, NVS ani Telegrama; kazda sekcja jest zamknieta;
      kazdy blok pamieci roboczej zwolniony; pobierzFirmwarePanelaZGithub()
      zawsze dostaje pusty tekst bledu

Krok [A4] skryptu porownuje blok decyzji z kopia, ktorej uzywaja testy
panelu LCD (LCD-Centrali/testy_host/kod_centrali/panel_ota_blok.inc - to samo
repozytorium). Tam prawdziwy firmware panelu v1.4.0 rozmawia z tym blokiem
jako "centrala". Roznica = testy panelu sprawdzaja inny kod niz ten w pliku
centrali; skrypt konczy sie wtedy bledem. Po SWIADOMEJ zmianie bloku:
    AKTUALIZUJ_WZORCE=1 ./uruchom_testy.sh
i ponownie testy panelu (LCD-Centrali/testy_host/uruchom_testy.sh).
Gdy paczka centrali jest rozpakowana osobno (katalogu panelu nie ma), ten
krok jest pomijany.

Czulosc testow (sprawdz_czulosc_ota.py): 43 razy psuje jedna rzecz w
wycietym kodzie (na kopii) i sprawdza, ze testy to wykrywaja. Wynik z dnia
wydania: ../WYNIKI/testy-czulosc.txt.

Prawdziwe obrazy (sprawdz_prawdziwe_obrazy.sh): w testach powyzej "program
ESP32" buduje sam test, wedlug tego samego opisu formatu co kod. Ten skrypt
pobiera trzy pliki zbudowane narzedziami Espressif (repozytorium esptool,
test/images) i przepuszcza je przez panelOtaSkan*() wyciete z pliku centrali,
a w pelnym repozytorium takze przez kod panelu v1.4.0. Sprawdza, ze obraz dla
ESP32 z suma na koncu jest przyjety z tym samym identyfikatorem po obu
stronach, a obraz bez sumy i obraz dla innego ukladu - odrzucone.
Wynik z dnia wydania: ../WYNIKI/prawdziwe-obrazy.txt.

Czego testy A NIE sprawdzaja
  - pobierzFirmwarePanelaZGithub(): pobierania pliku z GitHuba i zapisu na
    dysk centrali. W tescie jest podstawiona. Nowe linie w niej (zbieranie
    poczatku i konca pliku, odrzucenie zlego pliku, tekst bledu 404) sa
    sprawdzane tylko co do typow - sprawdz_naglowki_esp32.sh wycina je
    z pliku i kompiluje prawdziwymi naglowkami.
  - Jednolinijkowych wywolan w starszym kodzie: w trasach HTTP, tgTaskFn(),
    setup(), obsludze komend Telegrama i Firebase (WYNIKI/zakres-zmian.txt).
  - Stosu zadania Telegrama. Kod trzyma teksty i kopie stanu na stercie, a
    wysylki wychodza z funkcji o malych ramkach - ale ile stosu zostaje na
    plytce, pokaze dopiero hwm_tg w logu centrali (tag=HEALTH-A).
  - Prawdziwego serwera HTTP, NVS, Telegrama, wspolbieznosci trzech zadan
    (serwer WWW, zadanie Telegrama, setup). Test sprawdza zasady sekcji
    krytycznej, ale wyscigu dwoch rdzeni PC nie odtwarza.
  - Niczego na plytce.


B. WYKRESY DLA PANELU (test_panel_hist.cpp, 2697 sprawdzen)
-----------------------------------------------------------
Funkcja historiaPanelZbudujCsv() - GET /api/history/panel. Kod jest
identyczny z 3.58.5: krok [1] czesci B skryptu wycina go z pliku 3.58.6
i z pliku 3.58.5 (../../v3.58.5-panel-wykresy-po-restarcie/, gdy jest
w repozytorium) i porownuje bajt w bajt, poza linia z numerem wersji.

Dwa rodzaje sprawdzen:
  a) przypadki z wynikiem policzonym recznie,
  b) porownanie z niezaleznym liczeniem "sekunda po sekundzie" na 1834
     losowych zestawach danych.
Pelny opis przypadkow (1-16): tests/README.txt w paczce 3.58.5.

Krok [4] czesci B porownuje 11 odpowiedzi centrali z plikami, na ktorych
testowany jest firmware panelu (LCD-Centrali/testy_host/wzorce_centrali/).

Czulosc (sprawdz_czulosc.py): 14 celowych bledow w kodzie dodanym w 3.58.5.


SPRAWDZENIE PRAWDZIWYMI NAGLOWKAMI (sprawdz_naglowki_esp32.sh)
--------------------------------------------------------------
Te same wycinki kompilowane w trybie "tylko sprawdz" z prawdziwymi
naglowkami: arduino-esp32 3.0.7 (domyslnie) albo 3.3.8 (RDZEN=3.3.8),
ESPAsyncWebServer 3.10.3, AsyncTCP 3.4.10. Wykrywa pomylke w nazwie albo
sygnaturze funkcji biblioteki, ktorej atrapa by nie zauwazyla. To nie jest
build PlatformIO: nie powstaje program i nie jest sprawdzany caly plik.
Do wycinka czesci A dolaczone sa linie [v3.58.6 PANEL-OTA] wyciete
z pobierzFirmwarePanelaZGithub(), w funkcji o tych samych parametrach
i typach zmiennych lokalnych (reszta tej funkcji wymaga calej centrali).
Wynik z dnia wydania: bez bledow i bez ostrzezen na obu rdzeniach.


PLIKI
-----
  uruchom_testy.sh            czesc A i B
  test_panel_ota.cpp          testy A
  test_panel_hist.cpp         testy B
  wytnij_blok.py              wycinanie kodu z pliku centrali (--ota,
                              --ota-blok, --ota-blok-sprawdz, --ota-esp32
                              dla A; bez opcji, --sprawdz, --esp32 dla B)
  sprawdz_czulosc_ota.py      czulosc testow A
  sprawdz_czulosc.py          czulosc testow B
  sprawdz_naglowki_esp32.sh   prawdziwe naglowki ESP32
  sprawdz_prawdziwe_obrazy.sh prawdziwe pliki .bin przez kod oceniajacy plik

TESTY PANELU v3.58.2 — ATRAPA SERWERA (bez sprzętu)
===================================================

Co to jest
----------
Testy interfejsu nowego panelu (WIFI_LOCAL_PANEL_HTML) i przełącznika stron,
uruchamiane w Node.js na komputerze — bez ESP32 i bez prawdziwej sieci.
Panel i stara strona są wyciągane WPROST z ../src/main_centrala.cpp
(raw-stringi PROGMEM), więc testowany jest dokładnie ten kod, który
wgrywasz na płytkę. Atrapa ESP32 (mock-esp.js) odtwarza kontrakt HTTP
centrali 1:1 wg main_centrala.cpp (semantyka /api/lan/cmd i /api/lan/ack
przeniesiona z lanKomendaPrzyjmij() i lanAckOdpowiedz()), a atrapa Firebase
przechwytuje wywołania REST RTDB — testy nie dotykają prawdziwego internetu.

Wymagania: Node.js >= 18 (testowane na v22, Windows/Linux).

Uruchomienie (PowerShell):
    cd tests
    npm install        # raz; pobiera jsdom z rejestru npm
    node run-tests.js

Scenariusze (85 asercji):
    T1  panel otwarty z centrali: LIVE bez logowania, dane z /api/lan/status,
        ZERO zapytań do Firebase, karta "Starsza strona lokalna" widoczna,
        przycisk "STARA WERSJA" w pasku pulpitu, diagnostyka sesji
        (Wersja centrali + Źródło danych = lokalne API)
    T4  utrata centrali (LAN) i powrót: banner "Brak połączenia ze sterownikiem
        w sieci lokalnej", chip BRAK SESJI, powrót do LIVE
    T2  komendy lokalne: POST /api/lan/cmd z nagłówkiem X-Piec-Token,
        ACK 202→done, NACK z powodem, HTTP 409 przy zajętym slocie,
        dokończenie komendy po zwolnieniu slotu
    T3  zły token komend: HTTP 403 + komunikat, próba widoczna w atrace;
        [v3.58.2] po 403 okno połączenia otwiera się automatycznie z polem
        tokenu (prefill), zapis "ZAPISZ TOKEN" zamyka okno i zapamiętuje
        token (localStorage), kolejna komenda idzie z poprawnym
        X-Piec-Token i dostaje ACK
    T5  praca ZDALNA: auto-logowanie Firebase, status z RTDB, komenda
        PATCH /piec/cmd + ACK /piec/ack, zero komend lokalnych; karta
        i przycisk "STARA WERSJA" widoczne także zdalnie, klik = toast
        ze wskazówką (bez nawigacji), źródło danych = Firebase RTDB
    T5b panel zdalny bez internetu: BRAK SESJI + oryginalny komunikat
        o Firebase, tryb symulacji dostępny (zachowanie 3.57.9)
    T6  przełączanie stron: GET /stara 200 text/html, przycisk NOWY PANEL → /,
        token akcji wstrzyknięty, MENUS.stara nawiguje na /stara
    T7  stara strona: ładuje się, odpytuje /api/status, IP w pasku, zero
        błędów JS
    T8  brama statyczna main_centrala.cpp: wersja v3.58.2, changelog na górze,
        domknięte literały raw-string, rozsądny rozmiar zmiany
        (porównanie z oryginałem 3.57.9 wymaga zmiennej CPP_ORIG_PATH)
    T9  [v3.58.1] sonda lokalna: 3 próby z odstępem 1,5 s (reprodukcja
        usterki z v3.58.0 — jedno niepowodzenie blokowało tryb lokalny),
        cooldown 30 s dla automatycznych ponowień, samonaprawa po
        powrocie centrali (force=true) → LIVE, nawigacja do /stara

Ustawienia środowiska (opcjonalne):
    CPP_PATH        ścieżka do testowanego main_centrala.cpp
                    (domyślnie ../src/main_centrala.cpp)
    CPP_ORIG_PATH   ścieżka do oryginału v3.57.9 (włącza porównanie diff w T8)

Ważne — instrumentacja testowa:
    Testy wyprowadzają funkcje wewnętrzne panelu (sendCommand, pollFirebase,
    openMenu, MENUS, espPanelIsServed) do window.__PANEL_TEST__ DOPISUJĄC
    kilka linii do skryptu PRZED jego uruchomieniem w jsdom. Plik
    main_centrala.cpp pozostaje w 100% nietknięty — testy wołają oryginalne,
    niezmodyfikowane funkcje panelu.

Czego te testy NIE robią:
    - nie kompilują firmware (to robi PlatformIO w projekcie użytkownika),
    - nie testują sprzętu, WiFi, czujników, Firebase centrali ani Telegrama,
    - nie zastępują testu na gołej płytce ESP32-S3 (scenariusz w
      CZYTAJ-NAJPIERW.txt, krok 3).

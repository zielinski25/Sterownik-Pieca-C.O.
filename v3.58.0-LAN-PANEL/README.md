# Firmware 3.58.1 — panel lokalny bez Firebase + powrót starej strony

Data: 2026-11-10 · baza: **3.57.9** (SHA-256 `src/main_centrala.cpp`:
`e263437b1fb850cb202c4696404a41b7777b0f12aeef928c7e57788fb2fc158c`) ·
zastępuje **3.57.9 → 3.58.0 → 3.58.1** (poprawka po pierwszym teście użytkownika)

Zmienia się **jeden plik**: `src/main_centrala.cpp`. Pliki `.h` bez zmian,
`src/secrets.h` nietknięty. Biblioteki i `platformio.ini` bez zmian
(dokładnie te same co przy kompilacji 3.57.9).

> **v3.58.1 (poprawka):** naprawia wykrywanie trybu lokalnego (sonda
> `/api/lan/status` ma teraz 3 próby + samonaprawę po powrocie centrali —
> w 3.58.0 jedno chwilowe niepowodzenie blokowało tryb lokalny na całą
> sesję) oraz **przełącznik starej strony jest teraz ZAWSZE widoczny**:
> nowy przycisk „⇄ STARA WERSJA" w pasku pulpitu + karta w Ustawieniach
> (zdalnie kliknięcie pokazuje wskazówkę zamiast nic nie robić). Karta
> sesji pokazuje dodatkowo **wersję centrali** i **źródło danych**
> (lokalne API czy Firebase) — od razu widać, co naprawdę działa.

---

## Co ta wersja robi

**1. Nowy panel działa w sieci lokalnej BEZ Firebase i bez internetu.**
Po połączeniu z WiFi/AP centrali i otwarciu `http://<IP>/`:

- stan pobierany jest bezpośrednio z centrali: `GET /api/lan/status`
  (dokładnie ten sam JSON co `/piec/status` we Firebase — wspólny builder
  `zbudujJsonStatusPelny()`),
- komendy idą bezpośrednio do centrali: `POST /api/lan/cmd` + potwierdzenie
  `GET /api/lan/ack?id=...` — ta sama trasa LAN, z której od v3.47.0 korzysta
  aplikacja Android,
- żadne logowanie nie jest wymagane do **odczytu** danych.

To endpointy, które **już istniały** w firmware (dodane w v3.47.0 pod aplikację
Android) — nowy panel po prostu z nich teraz korzysta. Logika centrali,
automatyka, Firebase, Telegram, LCD: **bez zmian**.

**2. Firebase zachowane dla pracy zdalnej.** Panel otwarty z pliku
(`file://`) lub z innej domeny pracuje jak dotychczas: REST RTDB + logowanie
UserAuth (`/piec/status`, `/piec/cmd`, `/piec/ack`). Wykrywanie trybu jest
automatyczne: strona pyta własny origin o `/api/lan/status`; odpowiedź z polami
`lan:true`/`lan_api:1` = tryb lokalny, brak odpowiedzi = tryb Firebase.
**Od v3.58.1** sonda ma 3 próby (odstęp 1,5 s) i samonaprawia się po powrocie
do aplikacji / odzyskaniu sieci / kliknięciu „STARA WERSJA" — chwilowe
niepowodzenie nie blokuje już trybu lokalnego na całą sesję.

**3. Przełącznik nowa ↔ stara strona lokalna.** Starszy interfejs
(`wifiZbudujStrone()`, wycofany z `GET /` w v3.40.0) wraca jako **`GET /stara`**
— niezmieniony od tamtej wersji. Przełączanie w obie strony:
przycisk **„⇄ STARA WERSJA"** w pasku pulpitu nowego panelu (zawsze widoczny),
karta **„Starsza strona lokalna"** w Ustawieniach (zawsze widoczna) oraz
przycisk **„✦ NOWY PANEL"** w pasku nagłówka strony starej. Panel otwarty
zdalnie (przez Firebase, spoza centrali) po kliknięciu przełącznika pokazuje
wskazówkę, jak otworzyć panel z adresu centrali — zamiast cicho nic nie robić.

---

## Zmiany w liczbach

| rodzaj | zakres |
|---|---|
| JS nowego panelu (wewnątrz literału `WIFI_LOCAL_PANEL_HTML`) | tryb LAN: sonda `/api/lan/status` (3 próby + samonaprawa), gałąź LAN w `pollFirebase()`, `sendCommand()`, `tryAutoLogin()`, komunikaty UI, karta + przycisk przełącznika (zawsze widoczne), diagnostyka sesji (wersja + źródło danych) |
| HTML starej strony (`WIFI_STRONA_HTML_HEADER`) | 1 link „NOWY PANEL" w pasku nagłówka |
| C++ poza HTML | **jedyna nowa logika**: rejestracja trasy `GET /stara` (15 linii) + podbicie `FIRMWARE_VERSION` + wpisy changeloga |

Razem 351 zmienionych linii w 19 blokach diffu, w tym tylko ~62 poza
literałami HTML (changelog 45, wersja 1, trasa 15 + komentarz).
Pełny diff: `main_centrala-v3.57.9-do-v3.58.1.patch`.

## Czego NIE zmieniono

- Zabezpieczeń: komenda lokalna wymaga nagłówka `X-Piec-Token` z tym samym
  `CMD_TOKEN` (z `secrets.h`) co `/piec/cmd` — odrzucenie = HTTP 403, jak dotąd.
  Status lokalny jest jawny (bez tokenu) — identycznie jak w aplikacji Android.
- Logiki pieca, automatyki, czujników, wyjść, ustawień, NVS, Firebase,
  Telegrama, telemetrii, LCD, OTA, formatu `/piec/status`, `/piec/cmd`,
  `/piec/ack` i wszystkich istniejących endpointów.
- Wyglądu stron: zmiany UI to wyłącznie teksty stanu połączenia (prawdziwe
  dla trybu LAN), karta w Ustawieniach, przycisk „⇄ STARA WERSJA" w pasku
  pulpitu i jeden przycisk w starej stronie.

## Token komend — co trzeba wiedzieć

Odczyt danych lokalnie działa bez czegokolwiek. **Wysyłanie komend**
(pompa/serwo/ustawienia...) wymaga tokenu komend — `SECRET_CMD_TOKEN` z
`secrets.h`, dokładnie tego samego, jakiego wymagał tryb Firebase. Panel czyta
go z pamięci przeglądarki (klucz `piec_cmd_token`, jak dotychczas — panel nie
dodaje żadnego nowego okna ani pytania o token). Zły token = HTTP 403
z czytelnym komunikatem.

## Testy

- **Atrapa serwera (bez sprzętu)**: `tests/run-tests.js` — 78 asercji,
  9 scenariuszy (jsdom + mock ESP32 + mock Firebase, zero prawdziwej sieci):
  start lokalny bez Firebase, komendy z ACK/NACK/409/403, utrata i powrót
  połączenia, praca zdalna przez Firebase (w tym toast zamiast nawigacji
  przy kliku w przełącznik), przełączanie stron, ładowanie starej strony,
  brama statyczna pliku oraz **nowy T9: sonda z 3 próbami + samonaprawa**
  (reprodukcja usterki zgłoszonej w 3.58.0). Wynik: **78/78 OK**
  (pełny log: `WYNIKI/testy-atrapa-serwera.txt`).
- **Sprzęt (ESP32-S3): NIE testowane** — wersja nie była wgrywana na płytkę
  ani na piec. Testy na gołej płytce robi użytkownik.

## Znane ograniczenia

- Zakładka „Pogoda" pobiera dane bezpośrednio z `api.open-meteo.com`
  w przeglądarce — bez internetu pokazuje dane wygenerowane (jak dotychczas;
  centrala ma własny cache `/api/prognoza`, ale panel z niego nie korzysta —
  zachowanie oryginalne, celowo niezmienione).
- Komenda lokalna i komenda Firebase dzielą jeden slot wykonawczy w centrali
  (tak działa `wykonajKomendePanelu()` od v3.47.0): druga komenda w trakcie
  wykonywania pierwszej dostaje HTTP 409 i toast „spróbuj za chwilę".
- Stara strona jest budowana w RAM przy każdym żądaniu (mechanizm sprzed
  v3.40.0) — przy rzadkim używaniu bez znaczenia praktycznego.

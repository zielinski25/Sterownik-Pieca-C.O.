# Etap 221 — prawdziwy serwer HTTP biblioteki, uruchomiony na komputerze

## Po co ten etap

Do tej pory cały nasz kod HTTP (trasy `/history*`, zapinka historii, bilety)
był sprawdzany **na naszych własnych atrapach** biblioteki. Etap 220 pokazał,
że atrapy różnią się od prawdziwej biblioteki w 15 z 19 wywołań — same różnice
okazały się nieszkodliwe, ale wniosek był jasny: **atrapa nie jest dowodem**.

Ten etap robi następny krok: bierze **prawdziwe źródła biblioteki**
(`WebRequest.cpp`, `WebServer.cpp`, `WebHandlers.cpp`, `WebResponses.cpp` —
ten sam kod, który kompiluje się na urządzeniu), **prawdziwe `String`/`Print`/
`Stream`** z ArduinoCore-API i **nasz kod wycięty dosłownie z v3.56.4**,
kompiluje to jednym `g++` i **podaje surowe bajty żądania HTTP** do prawdziwego
parsera. Odczytuje surowe bajty odpowiedzi — takie, jakie poszłyby do telefonu.

Innymi słowy: to nie jest test modelu. To jest test **prawdziwego kodu
biblioteki** na komputerze.

## Co jest prawdziwe, a co jest nasze (granica)

| Warstwa | Źródło | Prawdziwe? |
|---|---|---|
| Parser HTTP (`_onData`, `_parseReqHead`, `_parseLine`) | `biblioteka/espasyncwebserver/src/WebRequest.cpp` | **TAK** — z przypiętego commita biblioteki |
| Wybór handlera (`_attachHandler`, `AsyncURIMatcher::matches`) | `WebServer.cpp` | **TAK** |
| Budowa i wysyłka odpowiedzi (`_respond`, `_ack`, `_assembleHead`) | `WebResponses.cpp` | **TAK** |
| `String`, `Print`, `Stream`, `IPAddress` | `biblioteka/arduino-core-api` | **TAK** — prawdziwe ArduinoCore-API |
| Nasze trasy, bramka, `HistoriaPobranie.h`, `HistoriaPin.h`, bilety | wycięte z v3.56.4 (`zrob_ekstrakcje.py`) | **TAK** — co do bajtu, SHA pilnuje `MANIFEST.json` |
| Gniazdo sieciowe | `platforma/AsyncTCP.*` | nie — bufor w pamięci zamiast lwIP |
| Dysk „flash” | `platforma/FS.cpp` + `KorzenCalls` w `Test.cpp` | nie — katalog z prawdziwymi plikami zamiast LittleFS |
| `cbuf` (bufor `AsyncResponseStream`) | `platforma/cbuf.h` | nie — nasze trasy go nie używają |
| `authenticate()` biblioteki (Basic/Digest) | `platforma/AuthZaslepki.cpp` | nie — nasze trasy wołają własne porównanie tokenu, nie `authenticate()` |
| `itoa`/`ltoa`/`dtostrf`, `millis`, `esp_random` | `platforma/Platform.cpp` | nie — to robi rdzeń ESP32 |

Emulacja gniazda jest **celowo prosta, ale wierna w kolejności bajtów**:
`write()` = `add()` + `send()` (jak w prawdziwym AsyncTCP), `space()` = 1460 B,
a test „pompuje” potwierdzenia TCP, żeby biblioteka dosyłała ciało w kawałkach
tak jak w sieci.

## Co ten etap **udowadnia**

* Że nasz kod z v3.56.4 **kompiluje się** przeciw prawdziwym nagłówkom biblioteki
  (nie tylko przeciw atrapie) — to był cały powód istnienia etapu 220.
* Że **prawdziwy parser** czyta nasze żądania i że **prawdziwy dyspozytor**
  wybiera właściwy handler: `/history`, `/history/long` i `/history/link`
  nie przechwytują się nawzajem (błąd z etapu 218).
* Że odpowiedź CSV ma prawdziwe nagłówki: `Content-Type: text/csv`,
  `Content-Disposition: attachment; filename="historia-krotka.csv"` /
  `"historia-dluga.csv"` i `Content-Length` równy rozmiarowi pliku,
  a ciało odpowiedzi **bajt w bajt** równa się plikowi na dysku.
* Że odmowy (401/405/503/404/413) są prawdziwymi odpowiedziami HTTP
  z właściwym kodem i nagłówkiem, a nie zerwaniem połączenia.
* Że bilet z panelu działa **raz**, wygasa po 5 minutach i jest związany
  ze strumieniem, dla którego został wydany.
* Że drugie pobranie w trakcie pierwszego dostaje 503, a nie drugi strumień
  z tego samego pliku.

## Czego ten etap **NIE** udowadnia

* **Nie zastępuje kompilacji PlatformIO na Twoim komputerze.** Tutaj kompiluje
  `g++` na Linuksie (x86), a nie `xtensa-esp32-elf-g++` z Arduino-ESP32.
  Inny kompilator = inne ostrzeżenia, inne makra rdzenia.
* **Nie sprawdza prawdziwego lwIP, WiFi, LittleFS ani pamięci ESP32.** Gniazdo
  i dysk są emulowane.
* **Nie sprawdza wyglądu strony na telefonie** ani CSS.
* **Nie sprawdza zachowania pod prawdziwym obciążeniem** (kilka połączeń naraz,
  zerwanie w połowie pobierania przez sieć).

## Jak uruchomić

```bash
python3 -B narzedzia/prawdziwy-serwer221/run.py
```

Wynik powinien kończyć się linią:

```
PASS brama221 kontrol=28 profili=2 mutantow=12 czas=...
```

Skrypt sam:
1. odbudowuje v3.56.4 z v3.55.8 + patch 218 i sprawdza SHA pliku
   (`f89874f3…8e38`), potem sprawdza, że wycięte fragmenty to dokładnie ten tekst,
2. sprawdza, że cztery nasze nagłówki w teście są bajt w bajt te same co w repo,
3. kompiluje prawdziwe źródła biblioteki i test,
4. uruchamia dwa profile (z tokenem i bez tokenu),
5. psuje po kolei 12 miejsc w kodzie i sprawdza, że test to wykrywa.

## Kontrole

**Profil z tokenem** (`ETAP221_TOKEN` ustawiony) — 24 kontrole:

| Kontrola | Czego pilnuje |
|---|---|
| S00_start | serwer wystartował, pliki historii istnieją |
| A01_dluga_200 | `/history/long` trafia do długiej historii (nie do krótkiej trasy) |
| A02_krotka_200 | `/history` zwraca krótką historię, ciało = plik |
| A03_naglowki_csv | `text/csv` + `attachment; filename="historia-krotka.csv"` + `Content-Length` |
| A04_dluga_nazwa_pliku | długa historia ma nazwę `historia-dluga.csv` |
| B01_bez_tokenu_401 | brak nagłówka → 401 + `WWW-Authenticate: Bearer` |
| B02_zly_token_401 | zły token → 401 |
| B03_male_litery_ok | `bearer` małymi literami też działa |
| B04_post_405 | POST → 405 + `Allow: GET` |
| C01_link_200_hex32 | `/history/link` wydaje 32 znaki hex |
| C02_link_no_store | bilet nie jest cache’owany (`Cache-Control: no-store`) |
| C03_bilet_dziala | bilet otwiera pobranie |
| C04_bilet_jednorazowy | ten sam bilet drugi raz → 401 |
| C05_bilet_dlugi_na_krotka_401 | bilet długi nie otwiera krótkiej trasy |
| C06_bilet_dlugi_na_dluga_200 | bilet długi otwiera długą trasę |
| C07_bilet_wygasl_401 | po 5 minutach bilet nie działa |
| D01_drugie_pobranie_503 | drugie pobranie w trakcie pierwszego → 503 |
| D02_pierwsze_dokonczone | pierwsze pobranie kończy się poprawnie |
| D03_po_zwolnieniu_200 | po zwolnieniu znacznika znów 200 |
| D04_magazyn_niegotowy_503 | magazyn zatrzymany → 503 |
| E01_tylko_potwierdzony_prefiks | wysyłany jest tylko potwierdzony prefiks pliku |
| E02_potwierdzony_wiekszy_niz_plik | licznik większy od pliku → wysyłane jest `min(licznik, plik)` |
| E03_plik_ponad_limit_413 | plik ponad limit dokumentu → 413 |
| E04_brak_potwierdzonych_404 | brak potwierdzonych bajtów → 404 |

**Profil bez tokenu** (`ETAP221_TOKEN` nieustawiony) — 4 kontrole:

| Kontrola | Czego pilnuje |
|---|---|
| F01_bez_tokenu_503 | pobieranie wyłączone → 503 (nie 401, nie 404) |
| F02_dluga_503 | `/history/long` też 503 |
| F03_link_503 | `/history/link` też 503 |
| F04_trasa_istnieje | trasa istnieje (503), a nie „nie ma takiego adresu” (404) |

## Mutanty (dowód, że test nie jest pusty)

Każdy mutant psuje **jedno** miejsce w kodzie i musi zrobić czerwono
kontrolę, która tego miejsca pilnuje:

| Mutant | Co psuje | Czerwone |
|---|---|---|
| M1_trasa_history_goly_napis | `/history` zarejestrowane gołym napisem (błąd z 218) | A01, A04, C01, C02 |
| M2_link_metoda_post | `/history/link` tylko POST-em | C01, C02, C03, C06 |
| M3_trasa_dluga_podaje_krotki | trasa długa podaje strumień krótki | A01, A04, C06 |
| M4_bramka_zawsze_krotka_sciezka | bramka zawsze wskazuje plik krótki | A01, C06 |
| M5_bez_content_disposition | brak `Content-Disposition` | A03, A04 |
| M6_bez_content_length | odpowiedź bez `Content-Length` | A01–A04 |
| M7_odpowiedz_bez_zrodla | `_sourceValid()` zawsze false | A01–A04 |
| M8_token_zawsze_zgodny | porównanie tokenu zawsze „zgodny” | B02 |
| M9_bez_kontroli_zajetosci | brak kontroli „trwa inne pobranie” | D01 |
| M10_zuzycie_biletu_zawsze_krotki | bilet zużywany zawsze jako krótki | C06 |
| M11_bez_cache_control | brak `Cache-Control: no-store` | C02 |
| M12_pusty_bufor_odpowiedzi | `_fillBuffer` zawsze 0 bajtów | A01, A02, B03, C03 |

## Pliki w tym katalogu

* `zrob_ekstrakcje.py` — wyciąga nasz kod z v3.56.4 i pilnuje, żeby to był ten sam tekst
* `run.py` — brama (kompilacja + profile + mutanty + `WYNIKI/run001/wynik.json`)
* `mutanty.py` — lista mutantów
* `Test.cpp` — scenariusze: surowe żądania → prawdziwy serwer → surowa odpowiedź
* `platforma/` — warstwa platformy na komputerze (opisana wyżej)
* `biblioteka/` — przypięte prawdziwe źródła + `SOURCES.json` z SHA każdego pliku
* `nasz/` — nasze nagłówki (bajt w bajt ze źródeł) i wycięte fragmenty v3.56.4
* `MANIFEST.json` — SHA fragmentów wyciętych z v3.56.4 i numer wersji pliku

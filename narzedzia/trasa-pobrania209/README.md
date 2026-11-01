# 209 — produkcyjna trasa pobrania historii

**NIE WGRYWAĆ. release_ready=false.** Bez zmian centrali 3.55.8, SDK i urządzenia.

## Problem, którego 208 nie zamykał

208 umiał przyjąć pobranie i wysłać je prawdziwą pętlą, ale **nikt go nie wywoływał**. Nie było decyzji
„to jest żądanie pobrania”, sprawdzenia poświadczeń, wyboru trasy ani odpowiedzi 404/405/401/503.
Test 208 sam budował żądanie i sam wołał adapter — czyli nie sprawdzał ani parsowania, ani wyboru trasy.

## Rozwiązanie

`DownloadRoute.h` — jedyne miejsce, które decyduje, czy żądanie staje się pobraniem:

1. **metoda** — tylko `GET`; inna metoda dostaje 405 (a nie ciche 404),
2. **poświadczenia** — wymagany `Authorization: Bearer <token>`; porównanie tokenu jest **stałoczasowe**
   (bez wczesnego wyjścia), więc czas odpowiedzi nie zdradza, ile znaków się zgadzało; brak nagłówka,
   inny schemat, inny token i inna wielkość liter → 401,
3. **pula** — brak miejsca → 503, powtórka tego samego żądania → 409, zły stan żądania → 400 (`Invalid`),
4. **magazyn / pamięć** — brak gotowości albo nieudane utworzenie odpowiedzi → 503,
5. sukces → `finish()` i wysyłka przez prawdziwą pętlę 199.

**Każda odmowa jest odpowiedzią HTTP, nie zerwaniem połączenia** — klient wie, co się stało. Odmowa nie
tworzy zapinki i nie dotyka pliku historii (test sprawdza zawartość pliku po odmowach).

`Real209Decl.inc` / `Real209Impl.inc` — wycięte **bez zmian** z przypiętej biblioteki
(`ESP32Async/ESPAsyncWebServer`, ref `c47917dc…`):

- `AsyncURIMatcher` i `WebRequestMethodComposite` z nagłówka,
- konstruktory i `matches` z `WebServer.cpp` (kandydat 206),
- `AsyncWebServer::_attachHandler` — kolejność: filtr → `canHandle` → `_catchAllHandler`,
- `AsyncCallbackWebHandler::canHandle` i `handleRequest` z `WebHandlers.cpp`.

`build.py` przy każdym przebiegu sprawdza, że każdy fragment jest bajtowo równy plikowi źródłowemu
i wypisuje `PASS 209 real matcher, real handler selection, real attach order extracted from pinned sources`.

Dzięki temu **wybór trasy nie jest już modelowany**: to prawdziwy matcher, prawdziwa maska metod i
prawdziwy porządek przypisania handlera. Modelowany pozostaje tylko sam obiekt żądań/odpowiedzi oraz TCP.

## Co rzeczywiście wykonano

- Prawdziwe parsowanie żądania (linia żądania, nagłówki, `urlDecode`, `_addGetParams`) z etapu 206 —
  żądanie powstaje z **surowych bajtów HTTP**, a nie z gotowych pól.
- Prawdziwy wybór trasy: `AsyncURIMatcher::exact("/history")`, maska metod `GET|POST|PUT|DELETE|HEAD`,
  `catchAll` odpowiadający 404.
- Prawdziwy 208 (zapinka 207, pula 2, generacje, terminy) i prawdziwa pętla wysyłki 199 z hookami 203.
- Rzeczywisty `RuntimeFilesystem.h` (197) i `FS.cpp` Arduino; backend plików modelowany, jak w 197–208.
- 23 grupy testów w 4 przebiegach (native i ASan+UBSan z LSan, po dwa tryby `CHUNK_INFLIGHT`).
- **12 skompilowanych mutacji wykrytych**: brak sprawdzania poświadczeń, brak sprawdzania schematu,
  token ignorowany, porównanie tylko pierwszego bajtu, odmowa bez odpowiedzi, brak sprawdzania metody,
  `Full` potraktowany jak sukces, `Unavailable` potraktowany jak 200, przyjęcie bez uprawnień,
  trasa jako prefiks zamiast dokładna, maska tylko `GET`, inny znacznik nagłówka.

## Zakres testów

Podział surowych bajtów żądania na każdej pozycji (1 B) i po każdej długości; inna ścieżka → 404 (bez
zapinki, bez tworzenia odpowiedzi); ukośnik na końcu `/history/` → 404 (matcher `exact`, nie prefiks);
`POST/PUT/DELETE/HEAD` → 405; brak nagłówka, zły token, `Basic`, goły token, pusty token, inna wielkość
liter, ten sam pierwszy znak, token dłuższy o jeden znak → 401; wielkość liter nazwy nagłówka i OWS;
`?x=1&y=2` obcinane przez prawdziwy parser; `/hist%6Fry` dekodowane do `/history`; dopisywanie w trakcie
wysyłki; dwa równoległe pobrania + trzecie 503 + ponowne użycie puli; rozłączenie w połowie (zapinka
zwolniona, rotacja możliwa); zatrzymanie magazynu w trakcie („source invalid”, nie czyste 200); magazyn
zatrzymany przed przyjęciem → 503 bez tworzenia odpowiedzi; brak pamięci → 503 i pula dalej działa;
termin (także przez przepełnienie zegara 32-bit) → zamknięcie i zwolnienie zapinki; spóźnione
potwierdzenie; 50 kolejnych żądań; okna 1/7/30 B; pusty plik; pobranie po rotacji; aborter na linii
żądania nie zostawia śladu; bezpośrednie testy porównania stałoczasowego.

## Granice — ważne przed użyciem

- To **nie jest** gotowy firmware: nie ma rejestracji trasy w prawdziwym `main_centrala.cpp`, nie ma
  konfiguracji tokenu, TLS ani limitu prób. Podłączenie do centrali to osobny krok.
- Klient TCP, znacznik nagłówka (`HEAD`) i **treść odpowiedzi błędu** (tylko kod, zero bajtów) są
  modelowane. Nie ma `AsyncBasicResponse` z tekstem błędu.
- `503` oznacza u nas **dwie różne rzeczy**: magazyn niegotowy i nieudane utworzenie odpowiedzi —
  adapter 208 nie rozróżnia ich w kodzie wyniku. To ograniczenie jest świadome.
- Token jest porównywany stałoczasowo, ale **nie ma limitu prób** — przy dostępnym porcie można go
  zgadywać bez ograniczeń. To zadanie dla warstwy transportu, nie dla tej trasy.
- Brak: `Range`, kompresji, szablonów, całej biblioteki i serwera, AsyncTCP, FreeRTOS, WDT, RAM-u,
  zaników zasilania, audytu całego programu i kwalifikacji urządzenia.

## Odtworzenie

Linux, Python 3, C++17:

```sh
python3 -B narzedzia/trasa-pobrania209/run.py
python3 -B narzedzia/trasa-pobrania209/mutacje.py
```

Raport: `WYNIKI/run002/wynik.json`; mutacje: `MUTACJE.json`. To nie są komendy wgrywania.

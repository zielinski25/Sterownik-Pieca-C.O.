# 208 — pobieranie historii przez prawdziwą pętlę wysyłki HTTP

**NIE WGRYWAĆ. release_ready=false.** Bez zmian centrali 3.55.8, SDK i urządzenia. To komponent offline,
nie gotowe pobieranie przez HTTP w firmware.

## Problem, którego 207 nie zamykał

207 daje **zapinkę** (pin) na dokładny, potwierdzony prefiks pliku historii, ale nic jej nie używało.
Nikt nie odpowiadał na pytanie: kto przyjmuje pobranie w kontekście sieciowym, kto tworzy odpowiedź
z ustaloną długością, kto czyta przez zapinkę i **kto ją zwalnia**, gdy klient zniknie, minie termin,
zawiedzie odczyt albo zabraknie pamięci na odpowiedź. Bez tego zapinka albo wisi na zawsze (blokując
rotację), albo zwalnia się za wcześnie i stara odpowiedź czyta nowsze dane.

## Rozwiązanie

`DownloadAdapter.h` — jedna instancja na serwer, stały czas życia, **tylko kontekst sieciowy**:

- `begin(request, timeout, authorized)` sprawdza kolejno: brak zgody → `Forbidden`; żądanie już wysłane,
  wstrzymane, mające odpowiedź, rozłączone albo bez klienta, termin zerowy lub zbyt duży → `Invalid`;
  powtórne przyjęcie tego samego żądania → `Conflict`; brak wolnego miejsca → `Full`.
  Dopiero potem powstaje zapinka 207 (`Snapshots::open`).
- Po zapince odczytywana jest **raz** jej długość (`remaining`) i tworzona jest odpowiedź o stałej
  długości `Content-Length` (`SnapshotResponse.h`). Każdy bajt treści pochodzi z zapinki, nie z pliku
  „na teraz”.
- `finish(handle, request)` przekazuje odpowiedź żądaniu; dalej działa **prawdziwa pętla 199**
  (`write_send_buffs`), a nie żaden skrót testowy.
- Zapinka jest zwalniana przy: zakończeniu wysyłki, rozłączeniu klienta, terminie, błędzie odczytu
  i nieudanym utworzeniu odpowiedzi. Ponowne użycie miejsca wymaga **rozłączenia i wygaśnięcia** słabego
  wskaźnika żądania, więc stara odpowiedź nie może czytać nowszej zapinki.
- Błąd magazynu **unieważnia źródło** (`_sourceValid()` → fałsz), a pętla 199 zamyka połączenie ze stanem
  `RESPONSE_FAILED`. Krótszy plik nigdy nie jest wysyłany jako poprawne 200.
- Adapter nic nie zapisuje, nie zmienia nazw i nie usuwa plików.

Pula ma 2 miejsca, uchwyty mają pokolenie. `pinned()` i `full()` służą do obserwacji w testach i diagnostyce.

## Co rzeczywiście wykonano

- **Prawdziwy kod 199**: `Transport208.inc` powstaje w `build.py` z pliku z etapu 199 przez **wycięcie
  wyłącznie** fabrycznego `_onPoll` i wstawienie kandydata z 203; `build.py` wypisuje
  `PASS 199 transport split: only stock _onPoll replaced by the 203 candidate`. Reszta pętli wysyłki,
  `_finished`, `_onAck` i `_onTimeout` są bez zmian.
- **Prawdziwy kod 203**: metody żądania (`send`, `abort`, `pause`, `_send`, `_onAck`, `_onDisconnect`,
  `installDeferred203`, …) pochodzą z etapu 203, razem z odroczonym `poll`/`gone`.
- **Prawdziwy 207**: `Snapshots.h` bez zmian, z jego zamkiem i rotacją.
- **Prawdziwy 197 + `FS.cpp`**: właściciel plików i rzeczywiste otwieranie/czytanie/szukanie/zamykanie.
- **Modelowane**: klient TCP (okno, limit przyjęcia, zamknięcie/przerwanie) i cztery bajty nagłówka
  (`HEAD`), żeby dało się sprawdzić, co dokładnie poszło po kablu. Model klienta odwzorowuje lwIP:
  poll przychodzi, gdy nic nie jest w locie, a każdy wysłany bajt wraca jako potwierdzenie z realną
  długością. To jest **model**, nie AsyncTCP.
- 21 grup testów, native i ASan+UBSan z LSan, w dwóch trybach `ASYNCWEBSERVER_USE_CHUNK_INFLIGHT`
  (0 i 1) — razem 4 przebiegi.
- **14 skompilowanych mutacji wykrytych** (`mutacje.py`): brak autoryzacji, przyjęcie żądania wstrzymanego,
  ignorowanie nieudanej instalacji hooka, brak podbicia pokolenia, zwolnienie trzymające zapinkę,
  uznanie cudzego żądania za własne, rozłączenie trzymające zapinkę, termin, który nigdy nie mija,
  błąd odczytu udający koniec, źródło zawsze ważne, długość spoza zapinki, brak zwolnienia po OOM,
  brak `Content-Length`, brak unieważnienia źródła. Mutacje równoważne (opisane niżej) zostały odrzucone,
  nie „zaliczone na siłę”.

## Zakres testów

Dokładne bajty 200 B i wielu linii; dopisywanie w trakcie wysyłki (nie rozszerza pobrania); odroczona
rotacja przy trwającym pobraniu i udana po zamknięciu; dwa równoległe pobrania z niezależnymi zapinkami
i trzecie odrzucone (`Full`); ponowne użycie miejsca z nowym pokoleniem; rozłączenie w połowie (zapinka
zwolniona, rotacja możliwa); termin (abort, `RESPONSE_FAILED`, zapinka zwolniona); przepełnienie zegara
32-bit; zatrzymanie magazynu w trakcie; zatrzymany magazyn przed przyjęciem (odmowa bez tworzenia
odpowiedzi); brak pamięci na odpowiedź; powtórne przyjęcie (`Conflict`); brak zgody (`Forbidden`);
złe argumenty (termin 0 i 0x80000000, żądanie wstrzymane, żądanie po `send(404)`); dwukrotne `finish`,
`finish` z cudzym żądaniem, `finish` po zwolnieniu; pusty plik; 50 pobrań pod rząd; małe okna TCP
(1/7/30 B przyjęcia na raz); spóźnione potwierdzenie po zakończeniu; pobranie po rotacji; błąd krótkiego
zapisu w historii (odmowa, nie 200).

## Granice — ważne przed użyciem

- To **nie jest** podłączenie do serwera: nie ma routingu, ścieżki `/history`, sprawdzania nagłówków
  żądania ani wysyłania odpowiedzi z handlera. 206 daje nagłówki i decyzję o trasie; tutaj jest tylko
  warstwa „przyjmij i wyślij”, wywoływana ręcznie.
- Nie ma `Range`, kompresji, szablonów i strumieniowania nieznanej długości. Długość jest zawsze znana
  z zapinki.
- Klient TCP i nagłówki są modelowane. To nie dowód na zachowanie AsyncTCP, FreeRTOS, WDT, RAM-u ani
  na to, że urządzenie wyśle plik pod obciążeniem.
- **`abort()` po `_send()` jest w tej bibliotece pustą operacją.** Dlatego po terminie adapter unieważnia
  źródło, a połączenie zamyka pętla wysyłki ze stanem `RESPONSE_FAILED`. Test tego pilnuje i nie liczy
  na `abort()`.
- Księgowanie „kredytów w locie” (`ASYNCWEBSERVER_USE_CHUNK_INFLIGHT`) jest zabezpieczeniem pamięci:
  zapis z poll-a i zapis z potwierdzenia w **tej samej** rundzie potrafią wyczerpać kredyty i wstrzymać
  wysyłkę. Model dlatego przepuszcza jedną rundę = jedno zdarzenie sieciowe. Na urządzeniu kredyty
  wracają wraz z potwierdzeniami; tej zależności nie da się tu udowodnić.
- Brak: całej biblioteki i serwera, AsyncTCP, FreeRTOS, ESP, RAM/WDT, braku zasilania, czyszczenia
  plików tymczasowych, pełnego audytu całego programu i kwalifikacji urządzenia.

## Odtworzenie

Linux, Python 3, C++11:

```sh
python3 -B narzedzia/pobieranie-http208/run.py
python3 -B narzedzia/pobieranie-http208/mutacje.py
```

Finalny raport: `WYNIKI/run002/wynik.json`; mutacje: `MUTACJE.json`. Nie są to komendy wgrywania.

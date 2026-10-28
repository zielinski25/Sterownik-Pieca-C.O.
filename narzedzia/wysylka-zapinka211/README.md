# 211 — zapinka historii w wysyłaczach dokumentów Telegram

**NIE WGRYWAĆ. `release_ready=false`.** Centrala 3.55.8 nietknięta, urządzenie nietknięte,
nic nie zostało wgrane ani wysłane na Telegram. To patch **kandydacki** plus testy hostowe.

## Po co to

Audyt 210 pokazał, że w centrali 3.55.8 tylko cztery funkcje dotykają plików historii, a dwie z nich
— `sendTelegramDocumentHistKrotka()` (linia 33817) i `sendTelegramDocumentHistDluga()` (33936) —
czytają plik **bez blokady**, **bez zapinki** i **z długością z cache**. Audyt nazwał to
„przypadkowo bezpiecznym”. Ten etap zamienia „przypadkowo” na **sprawdzone**.

### Co dokładnie jest nie tak w 3.55.8 (cztery usterki, każda z testem)

| # | gdzie | co się dzieje dziś | co robi 3.55.9 |
|---|---|---|---|
| 1 | `size_t totalLogSize = g_histShortSize;` (33828) | długość dokumentu bierze się ze **znacznika zapisu**, nie z pliku. Gdy znacznik jest **wyżej** niż plik (np. po zadaniu A1, po zewnętrznym skróceniu), ogłaszamy więcej bajtów, niż jest | długość = `min(znacznik, fstat)` **pod `logMutexem`** — wysyłamy prefiks potwierdzony przez oba źródła |
| 2 | `exists()` → `open()` (33822 → 33878) | między sprawdzeniem a otwarciem plik może zniknąć; wtedy `Content-Length` już poszedł | plik otwierany **raz**, długość z **tego samego** uchwytu; brak pliku = brak wysyłki |
| 3 | `size_t n = plik.read(...); if (n == 0) break;` | koniec pliku przed zapowiedzianą długością jest traktowany jak **normalny koniec** — ucięty dokument jedzie dalej, ogon multipart też, a `ok` może być `true` | `Short`/`Fault` = **zerwanie połączenia**, `TG_NET_FAIL`, `ok=false`, bez ogona |
| 4 | `if (zapisano == 0) break;` w pętli zapisu (33884) | jeśli gniazdo przyjmie **część** porcji i potem zero, resztę porcji **po cichu gubimy** i wysyłamy dalej | `zapisano != odczyt.bytes` = zerwanie połączenia |

Do tego dochodzi piąta rzecz, której nie było widać w audycie: **nikt nie ustalał długości pod tą samą
blokadą, której używa pisarz**. `history193::Engine::append()` rośnie porcjami po 256 B, więc `fstat`
bez `logMutexa` może zobaczyć **połowę linii**. Teraz `fstat` jest pod blokadą, a blokada jest
zwalniana **natychmiast po otwarciu zapinki** — nigdy na czas wysyłki (inaczej zapis historii czekałby
minutami).

## Rozwiązanie

Dwa elementy:

1. **`HistoriaPin.h`** — nowy nagłówek firmware'u (bez Arduino, bez FreeRTOS w środku, ~200 linii):
   `Pin::open(io, ścieżka, znacznik_zapisu, limit)` → jedno `open()`, jedno `fstat()`, długość
   `min(oba)`. Potem `read()` wydaje **wyłącznie** zapowiedziany prefiks i **nigdy** nie zamienia
   krótkiego odczytu ani błędu VFS na „koniec”. `close()` zwraca, czy zamknięcie zostało potwierdzone.
2. **Patch `main_centrala.cpp` v3.55.8 → v3.55.9** — pięć zmian tekstowych, obie funkcje wysyłające
   przepisane na zapinkę. Nic więcej: format wiadomości, boundary, podpisy, kolejność i treść
   multipart, pętla odpowiedzi, wszystkie `goto` i obsługa `tgSendMutex` **bez zmian**.

### Co patch zmienia poza samymi funkcjami

- `#include "HistoriaPin.h"` (linia 5128 po patchu),
- `static history193::SystemCalls historiaSys;` przeniesione **wyżej** (22756) — obie funkcje
  wysyłające stoją wcześniej w pliku niż sekcja `[v3.55.5 HISTORIA-IO]`, a potrzebują tego samego
  wywołania VFS, którym pisze `historiaFlushDoFS()`. Obiekt jest bez stanu, typ jest kompletny,
  w pliku zostaje **jedna** definicja,
- `FIRMWARE_VERSION` `"v3.55.8"` → `"v3.55.9"` (5562) i nowy wpis w nagłówku pliku.

## Co rzeczywiście wykonano

- **Prawdziwy kod, nie makieta.** `build.py` nakłada patch na plik o SHA-256 `3fb7510a…7a31`
  (ten sam, który audytował 210), sprawdza, że powstało **10 fragmentów diffu i nic więcej**,
  i wyciąga obie funkcje **bajt w bajt** do `Extracted211.inc`. Test kompiluje te funkcje bez
  żadnej zmiany; modelowany jest wyłącznie świat zewnętrzny (gniazdo TLS, semafory, VFS, czas,
  alokacja).
- **26 grup, 128 sprawdzeń, 0 błędów**, w trzech profilach: `-O2 -Werror`, `ASan+UBSan+LSan`,
  `-D_GLIBCXX_ASSERTIONS`.
- **12 skompilowanych mutacji wykrytych** (`mutacje.py`), w tym: długość z cache zamiast z pliku,
  ignorowanie `fstat`, krótki odczyt jako koniec, błąd VFS jako koniec, brak limitu rozmiaru,
  przyjęcie „zero potwierdzone”, zamknięcie, które nie zamyka, przyjęcie krótkiego zapisu do gniazda,
  brak przerwania po nieudanym ciele, przyjęcie bez zapinki, otwarcie bez blokady pisarza.
  Przed mutacjami ten sam układ przechodzi na kodzie niemutowanym (kontrola w `mutacje.py`).

### Zakres testów (co jest sprawdzone, a nie „powinno działać”)

Dokładne ciało i `Content-Length` przy zwykłej wysyłce; znacznik wyżej niż plik; znacznik niżej niż
plik; brak pliku; pusty plik; magazyn niegotowy / brak LittleFS / brak sieci; `logMutex` zajęty;
dopisywanie w trakcie wysyłki (ciało się **nie** rozszerza, plik rośnie dalej); plik skrócony przed
pierwszym odczytem; plik skrócony **w połowie** ciała; błąd VFS przy odczycie; okno gniazda 7 B;
gniazdo przestaje przyjmować w połowie; brak pamięci na bufor 4 KB; brak połączenia; zajęty
`tgSendMutex`; plik powyżej limitu Telegrama; odpowiedź bez `"ok":true`; historia długa (własny plik,
ogon, podpis, tag); **`logMutex` trzymany przy otwarciu i wolny przy wysyłce**; 50 wysyłek pod rząd
bez wycieku uchwytów; błąd zamknięcia zapinki; umowa `Pin` (koniec idempotentny, użycie po zamknięciu,
drugie `open`, brak pliku, zero potwierdzone, za duży, zły argument, krótszy plik = `Short`,
błąd odczytu = `Fault`).

## Granice — ważne przed użyciem

- **To nie jest dowód na urządzeniu.** Modelowane są: gniazdo TLS (`write` częściowy, `print`
  całościowy), semafory FreeRTOS, VFS (mapa nazwa→bajty), zegar i alokacja. **Nie ma** LittleFS,
  mbedTLS, AsyncTCP, WDT, PSRAM, prawdziwego `logMutexa` ani prawdziwego Telegramu.
- Test nie sprawdza, **jak długo** trwa wysyłka ani czy 4 KB bufor wystarcza przy prawdziwym TLS.
- Limit 50 MB (`kMaxDocumentBytes`) to limit Telegrama na dokument od bota; w praktyce nieosiągalny,
  bo historia jest przycinana do `HISTORIA_MAX_PROBEK` / `HISTORIA_DLUGA_MAX_PROBEK` linii.
- Zapinka **nie blokuje rotacji** — w 3.55.8 rotacja w czasie pracy jest wyłączona
  (`historiaWyczysc()` zwraca `false`), więc nie ma czego blokować. Gdy rotacja wróci, trzeba
  dopisać odroczenie jak w etapie 207.
- `Short`/`Fault` kończą się **zerwaniem połączenia** — Telegram dostaje niekompletne żądanie
  i nie tworzy dokumentu. To jest zamierzone: lepiej brak dokumentu niż dokument ucięty w połowie
  z `ok:true`.
- Wysyłacz może teraz **odmówić** wysyłki, gdy `logMutex` jest zajęty dłużej niż 200 ms.
  Przy obecnym zapisie (jedna próba na 5 minut) to praktycznie nieosiągalne, ale to zmiana zachowania.
- Nie ma: `Range`, kompresji, ponowień, limitu prób, TLS-keepalive, wpływu na `/api/history*`.

## Jak to kiedyś wgrać (NIE teraz)

To jest polecenie **do wykonania na Twoim komputerze**, nie tutaj i nie przeze mnie:

```sh
cd <katalog z firmware>
patch -p1 --dry-run -i main_centrala-v3.55.8-to-v3.55.9.patch   # musi być czysto
patch -p1 -i main_centrala-v3.55.8-to-v3.55.9.patch
sha256sum main_centrala.cpp    # oczekiwane: 28545285b7f11fe48e051cf007c1ee5cd44fcf41260e4ec65ed88f34bc31a49e
```

Plik `HistoriaPin.h` musi leżeć obok `CheckedHistoryIO.h`. Po patchu: `FIRMWARE_VERSION` = `v3.55.9`
(linia 5562). **Wgrywa użytkownik, nie agent.** Bez tego etapu nic nie jest wgrane.

## Odtworzenie

Linux, Python 3, C++17:

```sh
python3 -B narzedzia/wysylka-zapinka211/zrob_patch.py   # regeneruje patch z v3.55.8
python3 -B narzedzia/wysylka-zapinka211/run.py          # build.py + 3 profile testów
python3 -B narzedzia/wysylka-zapinka211/mutacje.py      # 12 mutacji
```

Raport: `WYNIKI/run001/wynik.json`; mutacje: `MUTACJE.json`. To nie są komendy wgrywania.

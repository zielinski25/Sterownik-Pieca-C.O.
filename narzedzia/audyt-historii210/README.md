# 210 — kto naprawdę dotyka plików historii w centrali v3.55.8

**NIE WGRYWAĆ.** To audyt statyczny, nie zmiana firmware'u. Centrala `3.55.8` (SHA-256 `3fb7510a…a31`),
SDK i urządzenie nietknięte.

## Po co to

Przez etapy 196–209 powstały klocki: blokada FS przy starcie, właściciel plików, zapinka pobrania,
adapter wysyłki, trasa HTTP. Powstało pytanie, na które nikt dotąd nie odpowiedział **na prawdziwym
kodzie**: *kto w centrali otwiera pliki historii, co go wtedy chroni i gdzie da się jeszcze dostać
porwany plik*. Bez tej odpowiedzi nie wiadomo, czy podłączenie trasy z 209 cokolwiek naprawia.

## Metoda

`skan.py` czyta `main_centrala.cpp` (46 777 linii), zamienia komentarze i literały (w tym `R"(...)"`)
na spacje z zachowaniem numerów linii, odtwarza **zakresy funkcji po nawiasach klamrowych** (586 funkcji),
a potem szuka wywołań `open/remove/rename/exists` z nazwą pliku i sprawdza, jakie prymitywy blokady
występują w ciele funkcji. Wynik: `INWENTARZ.json` i `RAPORT_LINIE.md`.

`sprawdz.py` to **24 twierdzenia sprawdzane maszynowo** — każdy fakt z tego raportu jest testem.
Jeśli firmware się zmieni bez aktualizacji raportu, test padnie i zmusi do przeglądu.

## Wynik: cały dostęp do plików historii

| co | gdzie | chronione czym | ocena |
|---|---|---|---|
| zapis (jedyny) | `historiaFlushDoFS()` (linia 45310) → `historiaIo.append(Stream::Short/Long, …)` | `logMutex` (200 ms) + `historiaDostepGotowy()` + `historiaIo.blocked()` | **dobrze**: jeden pisarz, jawna blokada, brak bezpośredniego `open(…,"a")` |
| odczyt przy starcie | `historiaWczytajZFS()` (45657), `historiaDlugaWczytajZFS()` (45740) | brak blokady, ale **przed** utworzeniem zadania historii (`historiaTaskWakeSem`, 46283) | **bezpiecznie**: single-thread na starcie |
| odczyt w czasie pracy | `sendTelegramDocumentHistKrotka()` (33817), `sendTelegramDocumentHistDluga()` (33936) | **nic**: `open(HISTORIA_PLIK,"r")`, pętla 4 KB, wysyłka do Telegramu minutami | **ryzyko** — patrz niżej |
| HTTP `/api/history`, `/api/history/long` | 28572, 28572+ | serwują **bufory RAM** (`historiaZbudujCsv()`), nie plik | **bezpiecznie** |
| czyszczenie / rotacja w czasie pracy | `historiaWyczysc()` (24350) | **wyłączone**: `return false;` bez I/O | brak rotacji = brak porwania przez `rename` |
| edytor plików z panelu | `historiaChronionaSciezka()` (22710) | odrzuca `HISTORIA_PLIK`, `HISTORIA_DLUGA_PLIK`, pliki `.req/.done` A1, `..`, `//`, `\` | **dobrze** |

## Ryzyko, które realnie zostaje

Dwa wysyłacze dokumentów Telegram czytają plik historii **bez blokady i bez zapinki**, podczas gdy
`historiaFlushDoFS()` może w tym samym czasie dopisywać na końcu (Core 0, pod `logMutex`).

Dlaczego to dziś nie boli: plik jest **tylko dopisywany**, a czytelnik bierze prefiks o długości
z cache (`totalLogSize`), więc dostaje spójny, starszy fragment. Dziś jest więc „przypadkowo bezpieczne”.
Psuje się w dwóch momentach:

1. **gdy rotacja/czyszczenie wróci** (207 właśnie po to ma zapinkę i odroczenie rotacji) — czytelnik
   może dostać plik zmieniony nazwą w połowie odczytu;
2. **gdy ogłoszona długość nie zgadza się z plikiem** — `Content-Length` liczony z cache, a pętla kończy
   się na pierwszym `read() == 0`; wystarczy krótszy plik, żeby wysłać mniej bajtów niż zapowiedziano
   i uciąć żądanie do Telegramu.

To jest dokładnie ten sam problem, który 207/208 rozwiązują — tylko w innym miejscu kodu.

## Co z tego wynika dla podłączenia trasy (209)

- Nowy endpoint pobrania **musi** iść przez zapinkę 207 (jak trasa 209), a nie przez `open(…,"r")`.
- Dwa wysyłacze Telegramu to **naturalni klienci tej samej trasy**: zamiast własnej pętli `open/read`
  powinny dostać czytelnik zapinki (albo przynajmniej `logMutex` + długość z pliku, nie z cache).
- `/api/history*` nie wymaga zmian — serwuje RAM.
- Czyszczenie historii jest wyłączone, więc pierwsza wersja trasy nie musi obsługiwać rotacji
  w czasie pracy; zapinka i tak ją zabezpiecza, gdy czyszczenie wróci.

## Czego ten audyt NIE sprawdza

- Nie uruchamia firmware'u: brak dowodu na zachowanie LittleFS, WDT, RAM-u i czasu odpowiedzi.
- Nie analizuje pozostałych plików (logi `log_a/log_b`, ustawienia, panel) — tylko historię.
- „Brak blokady w funkcji” to fakt statyczny; nie dowodzi, że wywołujący nie trzyma blokady wyżej.
  Dla obu wysyłaczy sprawdziłem okno ±25 linii wokół wywołania (44248) — nie ma tam `logMutex`.
- Liczba 586 funkcji to wynik heurystyki po nawiasach; wystarcza do przypisania wywołań, nie jest
  pełnym parserem C++.

## Odtworzenie

```sh
python3 -B narzedzia/audyt-historii210/skan.py
python3 -B narzedzia/audyt-historii210/sprawdz.py
```

Wyniki: `INWENTARZ.json`, `RAPORT_LINIE.md`, `KONTROLA.json`. Nie są to komendy wgrywania.

# Firmware 3.57.7 — Telegram: „brak pliku” to nie „błąd wysyłki”

Data: 2026-10-31 · baza: **3.57.6** · zastępuje **3.57.6**
Zawartość: **pełne źródła** — podmień cały folder `src` (16 plików), nie tylko `main_centrala.cpp`.

---

## Po co ta wersja

Zgłoszenie z 31.10, 09:37: po wyłączeniu zapisu historii, czyszczeniu i restarcie
centrali bot odpowiedział `hist_short.csv: wysyłka nieudana - kopiuję na SD`
(i to samo dla `hist_long.csv`) — **a w tej samej chwili `log_ustawien.txt`
wysłał się poprawnie**. Czyli sieć i Telegram działały, brakowało wyłącznie
dwóch plików historii, które przy starcie usunął kanał A1. Wszystko działało
zgodnie z ustawieniem, ale komunikat sugerował awarię.

## Co jest zmienione

| # | co | 3.57.6 | 3.57.7 |
|---|---|---|---|
| 1 | brak pliku historii | wołana wysyłka → „wysyłka nieudana” + próba kopii na SD | najpierw sprawdzenie, czy plik istnieje i nie jest pusty; jeśli nie ma → **zero prób wysyłki i zero kopii na SD** |
| 2 | treść komunikatu | jedna wersja, zawsze o błędzie | dwie, zależnie od stanu: „historia na flashu jest pusta (np. po czyszczeniu)” albo „zapis historii na flash jest WYŁĄCZONY — włącz go, aby pliki wróciły” |
| 3 | komentarz nagłówka o `histfs_czysc` | mówił o znaczniku w NVS | mówi prawdę: kanał A1 (`.req` / `.done`) |

**Bez zmian:** zapis historii, bramki (`historiaFlushDoFS` / `historiaDlugaFlushDoFS`),
NVS (`hist` / `fs_on`), kanał A1 i jego kolejność, format CSV, oba panele,
Firebase, OTA, nadzór, TLS.

## Testy (wszystkie zielone)

| zestaw | wynik |
|---|---|
| statyczne (`node narzedzia/testy-firmware/test-hist-fs.js`) | **38 / 38** — w tym 5 nowych dla 3.57.7 |
| symulacja C++ (kanał A1 + bramki zapisu) | **80 / 80** |
| symulacja panelu w DOM | **26 / 26** |
| patch `-p0` na 3.57.6 | plik bit w bit identyczny z paczką |

## Jak wgrać

1. Rozpakuj ZIP, podmień **cały** folder `src` w projekcie PlatformIO.
2. `pio run -t upload` albo OTA z panelu (karta „Aktualizacja”).
3. Po restarcie wersja w panelu i aplikacji: **v3.57.7**.

## Czego ta wersja NIE zmienia

Nie włącza z powrotem zapisu historii i nie odtwarza usuniętych plików.
Po czyszczeniu pliki powstają od nowa dopiero po włączeniu przełącznika:
`hist_short.csv` po ~5 minutach, `hist_long.csv` przy pierwszym zdarzeniu
lub domkniętej godzinie.

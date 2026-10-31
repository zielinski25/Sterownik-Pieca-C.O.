# Etap 222 — pokwitowanie

**Data:** 2026-10-28
**Wynik:** `PASS brama222 profili=2 mutantow=6 czas=37.1s`
**Paczka:** `ETAP-222-OD-PRZYCISKU-DO-PLIKU.zip` — 32 601 B, 14 wpisów, sha256 `3341bce0…5700`

## Co to jest

Ostatnie ogniwo, którego nikt wcześniej nie sprawdził: **prawdziwe gniazdo TCP**
i **prawdziwy skrypt panelu**. W jednym procesie stoi prawdziwy `AsyncWebServer`
(parser i dyspozytor z etapu 221), do niego prowadzi prawdziwe gniazdo na
`127.0.0.1`, a po drugiej stronie działa prawdziwy panel z v3.56.4 w silniku V8.

Klik „Krótka (24 h)” → `fetch /history/link?stream=short` → przekierowanie na
`/history?k=<bilet>` → pobranie → **plik CSV zapisany na dysku**, bajt w bajt
równy plikowi z „flashu”. To samo dla „Długa (archiwum)” → `historia-dluga.csv`.

## Wynik w liczbach

* 33 sprawdzenia w profilu z tokenem, 22 w profilu bez tokenu,
* 6 mutantów — **wszystkie wykryte**,
* ten sam link drugi raz → 401 (sprawdzone prawdziwym HTTP, nie modelem),
* bez tokenu → panel pokazuje „Nie udało się: HTTP 503” i nie pobiera.

## Ważne odkrycie

Mutant M5 (odpowiedź nie daje bajtów, a `Content-Length` obiecuje 81) ujawnił,
że biblioteka **nie zamyka połączenia** — klient wisi. Dlatego test ma budzik
25 s (bez niego wyszedłby „zielony”, bo nic by nie sprawdził), a `_sourceValid()`
w `HistoriaPobranie.h` musi mówić prawdę.

## Czego to nie zastępuje

Nie zastępuje przeglądarki (DOM jest modelem tylko tej części, której panel
używa — bez CSS i bez wyglądu) ani kompilacji PlatformIO na Twoim komputerze.

## Plik do wgrania

Bez zmian: **v3.56.4** z etapu 218. Etapy 219–222 nie zmieniły firmware’u.

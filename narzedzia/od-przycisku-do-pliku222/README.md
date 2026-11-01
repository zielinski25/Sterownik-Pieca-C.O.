# Etap 222 — od kliknięcia przycisku do pliku CSV, po prawdziwym TCP

## Po co ten etap

Etap 218 sprawdził styk „panel ↔ trasa” na **modelu** serwera. Etap 221
uruchomił prawdziwy kod biblioteki, ale zadania podawał **wprost do parsera**
(bufor w pamięci). Zostało jedno ogniwo, którego nikt jeszcze nie sprawdził:
**prawdziwe gniazdo sieciowe** i prawdziwy skrypt panelu, który naprawdę
wykonuje `fetch` i naprawdę przekierowuje przeglądarkę na plik.

Ten etap to zamyka: w jednym procesie stoi prawdziwy `AsyncWebServer`
(z prawdziwym parserem i dyspozytorem z etapu 221), do niego prowadzi
**prawdziwe gniazdo TCP** na `127.0.0.1`, a po drugiej stronie siedzi
**prawdziwy skrypt panelu** z v3.56.4 uruchomiony silnikiem V8 (Node).
Klik „Krótka (24 h)” → `fetch /history/link?stream=short` → przekierowanie
na `/history?k=<bilet>` → pobranie → **plik CSV zapisany na dysku**.

## Co jest prawdziwe

| Warstwa | Źródło |
|---|---|
| Parser żądań, dyspozytor, odpowiedzi | `biblioteka/espasyncwebserver` (commit `c47917d`, przez etap 221) |
| Nasze trasy, bramka, bilet, zapinka | wycięte z v3.56.4 (te same pliki co w etapie 221, SHA pilnowane) |
| Skrypt panelu (przycisk, fetch, przekierowanie) | `Panel222.html` = panel z etapu 218, wycięty z v3.56.4 |
| Silnik JavaScript | V8 (Node 22) — prawdziwy `encodeURIComponent`, prawdziwe Promises |
| Gniazdo | **prawdziwe TCP** (POSIX), `127.0.0.1` |
| Plik CSV | **prawdziwy plik** na dysku, czytany przez `open/stat/read` |

## Co jest nasze (i co to znaczy)

* **DOM** — `getElementById`, `addEventListener`, `style`, `textContent`
  są modelowane dokładnie w tym zakresie, jakiego panel używa. Każde użycie
  czegoś spoza modelu (np. `querySelector`) rzuca wyjątek i test jest czerwony.
  Model DOM **nie** sprawdza wyglądu ani CSS.
* **Połączenie biblioteki z gniazdem** — `platforma/AsyncTCP.*` z etapu 221:
  bufor w pamięci zamiast lwIP, a potwierdzenia TCP symulujemy (ile bajtów
  wyszło do gniazda, tyle „ack”).
* **`LocalFS`/„flash”** — katalog z prawdziwymi plikami zamiast LittleFS.

## Co ten etap udowadnia

* Panel z v3.56.4 **naprawdę** dostaje bilet (32 znaki hex) i **naprawdę**
  otwiera właściwą trasę — krótką dla „Krótka”, długą dla „Długa”.
* Odpowiedź przechodzi przez prawdziwe gniazdo z nagłówkami
  `Content-Type: text/csv` i `Content-Disposition: attachment; filename=…`,
  a zapisany plik jest **bajt w bajt** równy plikowi z „flashu”.
* Ten sam link **drugi raz daje 401** (bilet jest jednorazowy) — sprawdzone
  przez prawdziwe HTTP, nie przez model.
* Bez tokenu w `secrets.h` panel dostaje 503, pokazuje komunikat
  „Nie udało się: HTTP 503” i **nie** przekierowuje na pobranie.

## Ważne odkrycie z mutantów (M5)

Gdy `_fillBuffer` zwraca 0 bajtów, a nagłówek `Content-Length` obiecuje 81,
biblioteka **nie zamyka połączenia** — czeka na resztę danych. Klient wisi.
Dlatego:
* test ma budzik (25 s) i bez niego wyszedłby „zielony”, bo nic by nie sprawdził,
* `_sourceValid()` w `HistoriaPobranie.h` musi mówić prawdę: jeśli zapinka
  jest uszkodzona, odpowiedź musi zostać unieważniona, a nie „udawać koniec”.

## Jak uruchomić

```bash
python3 -B narzedzia/od-przycisku-do-pliku222/run.py
```

Oczekiwany koniec:

```
PASS brama222 profili=2 mutantow=6 czas=...s
```

Wymaga etapu 221 obok (`narzedzia/prawdziwy-serwer221` — biblioteka i platforma).

## Kontrole

**Profil „z tokenem”** (33 sprawdzenia):

| Kontrola | Czego pilnuje |
|---|---|
| skrypt panelu wykonuje się bez wyjątku | panel nie używa niczego spoza modelu DOM |
| 3 przyciski mają po jednym obsłużniku klik | struktura panelu |
| klik HISTORIA CSV otwiera panel | `display` z `none` na `block` |
| komunikat „Przygotowuję link…” | panel reaguje na klik |
| jedno zapytanie o bilet: `/history/link?stream=short` | właściwy strumień |
| przekierowanie `/history?k=<32 hex>` | właściwa trasa |
| pobranie 200, plik `historia-krotka.csv` | nagłówki odpowiedzi |
| plik na dysku **bajt w bajt** równy plikowi z centrali | cała droga danych |
| to samo dla „Długa (archiwum)” → `/history/long?k=<32 hex>` → `historia-dluga.csv` | długa historia |
| ten sam link drugi raz → 401 | bilet jednorazowy |

**Profil „bez tokenu”** (22 sprawdzenia): bilet → 503, brak przekierowania,
komunikat „Nie udało się: HTTP 503”.

## Mutanty

| Mutant | Co psuje | Jak się czerwieni |
|---|---|---|
| M1_panel_zawsze_krotka_trasa | panel zawsze otwiera `/history` | „Długa” nie trafia na długą trasę |
| M2_panel_inna_dlugosc_biletu | panel oczekuje 31 znaków | brak pobrania |
| M3_trasa_history_goly_napis | `/history` gołym napisem (błąd z 218) | bilet nie dochodzi do trasy |
| M4_bez_content_disposition | brak nazwy pliku | plik nie ma nazwy (`null`) |
| M5_pusty_bufor_odpowiedzi | odpowiedź nie daje bajtów | przekroczony czas — połączenie wisi |
| M6_bilet_nie_jednorazowy | bilet nie jest zużywany | drugi raz daje 200 zamiast 401 |

## Pliki

* `run.py` — brama (buduje bibliotekę i mostek, uruchamia profile i mutanty)
* `Most.cpp` — mostek TCP ↔ prawdziwy `AsyncWebServer`
* `panel222.mjs` — prawdziwy skrypt panelu w V8 + model DOM + prawdziwe HTTP
* `Panel222.html` — panel wycięty z v3.56.4 (ten sam co w etapie 218)
* `nasz/` — kod z v3.56.4 i nasze nagłówki (kopie 1:1 z etapu 221)
* `WYNIKI/run001/wynik.json` — wynik ostatniego przebiegu

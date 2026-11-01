# Etap 218 — „przycisk pobierania nie działał w ogóle” (v3.56.3 → v3.56.4)

Ten etap łapie błąd, którego **nie widział żaden wcześniejszy test**: trasa
`/history` była zarejestrowana gołym napisem, a taki napis w ESPAsyncWebServer
znaczy „pasuje też wszystko pod tym prefiksem”. Skutek na urządzeniu: po
wgraniu łatki z etapu 216 **żaden** przycisk pobierania historii by nie zadziałał
— panel dostawał 401 zamiast biletu.

## Co dokładnie było zepsute

| Warstwa | Co robiła | Skutek |
|---|---|---|
| panel (v3.56.3) | `fetch('/history/link?stream=short')`, potem `/history?k=…` | poprawnie |
| tabela tras (v3.56.3) | `/history` = goły napis = matcher `BackwardCompatible` | łapała `/history/link` **i** `/history/long` |
| handler `/history` | zawsze `Strumien::Krotki`, bez `?k=` → 401 „Brak poswiadczen” | panel pokazywał „Nie udało się: HTTP 401” |
| handler `/history/long` | nigdy nie wywoływany | długa historia nie do pobrania żadną drogą |

Dwie rzeczy naraz: serwer bierze **pierwszy pasujący** handler w kolejności
rejestracji (`_attachHandler`, `WebServer.cpp:142`), a goły napis to matcher
`^{uri}(/.*)?$` (`AsyncURIMatcher::matches`). Ten sam błąd, który w tym pliku
naprawiono już dla `/api/history*` ([FIX 17]) — nowe trasy go powtórzyły.

Testy 212/213 wołały handler **bezpośrednio** (`historiaPobranie.obsluz(...)`),
więc tabela tras nigdy nie była w nich używana. Ten etap dokłada właśnie tę
brakującą warstwę.

## Co jest prawdziwe, a co jest modelem

**Prawdziwe (kompilowane z tekstu firmware):**
- trzy rejestracje tras `/history*` wycięte **dosłownie** z `main_centrala.cpp`
  po patchu v3.56.4 → `Extracted218.inc` (generowany, nie pisany ręcznie),
- adaptery losowości/zegara i podpięcie biletów do trasy — też dosłownie z pliku,
- `HistoriaPobranie.h` v3.56.3 (z `Content-Disposition`) i `HistoriaBilety.h`,
- bramka i obiekt trasy z etapu 212 (`Extracted212.inc`),
- skrypt panelu — uruchamiany prawdziwym silnikiem JS (V8 przez `node:vm`).

**Model (świadomie i jawnie):**
- serwer HTTP: `AsyncURIMatcher` + `_attachHandler` przeniesione z `WebServer.cpp`
  (ref `c47917d`) — tylko to, czego używa ten plik (brak regexów, brak
  parametrów ścieżki; sprawdza to `build.py`),
- DOM w `panel218.mjs`: tylko `getElementById`, `addEventListener/click`,
  `style.display`, `textContent`, `window.location`, `fetch`. Każde użycie
  czegoś spoza modelu rzuca wyjątek i test jest CZERWONY — model nie może
  „po cichu udawać, że działa”,
- VFS w RAM, semafory, `esp_random` (deterministyczny LCG).

## Co sprawdza brama

`python3 -B narzedzia/panel-trasa218/run.py`

| Krok | Sprawdzeń | Co dokładnie |
|---|---|---|
| `build.py` | 10 PASS | v3.55.8 + 211…216 + 218 → v3.56.4, oba patche dają ten sam plik, tabela tras przed/po |
| `panel218.mjs` | 37 | prawdziwy skrypt panelu: klik „Krótka” → `/history?k=…`, klik „Długa” → `/history/long?k=…`, błędy 401/503/brak sieci |
| `Test.cpp` | 78 × 3 profile | te adresy podane prawdziwej tabeli tras: bilet → CSV == plik, nazwa pliku, bilet jednorazowy, wiązanie biletu ze strumieniem, 405 na POST, wygaśnięcie po 5 min |
| mutanty | 5 | każda zepsuta wersja MUSI zrobić bramę czerwoną |

Mutanty (wszystkie wykryte):

| Mutant | Co psuje | Po co |
|---|---|---|
| M1 | `exact("/history")` → `"/history"` | dokładnie błąd v3.56.3 → 35 błędów |
| M2 | `exact("/history/long")` → goły napis | długa trasa martwa |
| M3 | `exact("/history/link")` → goły napis | panel nie dostaje biletu |
| M4 | panel zawsze `/history` | błąd v3.56.2 (D2) |
| M5 | panel bez kontroli 32 znaków | bilet-śmieć zostałby otwarty |

Profile kompilacji: `native` (-O2 -Werror), `asan-ubsan` (`detect_leaks=1`),
`glibcxx-assertions`.

## Wynik

```
PASS panel218 sprawdzen=37 bledow=0
PASS trasa218 sprawdzen=78 grup=10      [native / asan-ubsan / glibcxx-assertions]
PASS brama218 profili=3 mutantow=5 sprawdzen=78 grup=10
```

Tabela tras w całym pliku (46 tras) policzona przez `trasy.py`:

| Plik | Martwe trasy |
|---|---|
| v3.56.3 | 2 — `/history/long` i `/history/link`, obie przechwycone przez `/history` |
| v3.56.4 | 0 |

## Co użytkownik ma zrobić

**Nie używać już łatki z etapu 216.** Wziąć **skumulowaną** z tego etapu:

```
patch -p1 -i main_centrala-v3.55.8-to-v3.56.4.patch
```

Jeśli ktoś już nałożył v3.56.3, wystarczy przyrostowa:

```
patch -p1 -i main_centrala-v3.56.3-to-v3.56.4.patch
```

`HistoriaPobranie.h` **bez zmian** — zostaje ten z etapu 216 (sha256 `b58d1a4c…`).

Po wgraniu: telefon → panel → „⬇ HISTORIA CSV” → „Krótka (24 h)” ma zapisać
`historia-krotka.csv`, a „Długa (archiwum)” — `historia-dluga.csv`.

## Czego ten etap NIE dowodzi

- że PlatformIO skompiluje cały plik (to użytkownik u siebie),
- że przeglądarka w telefonie narysuje przycisk (sprawdzana jest logika i adresy, nie CSS),
- że prawdziwy LittleFS i WiFi zachowają się jak model,
- że panel w innym kliencie (aplikacja) używa tych samych adresów.

## Zmiany w firmware

`main_centrala.cpp`: **+21 linii** (komentarz na górze, 3 rejestracje `exact()`,
komentarz przy `/history/link`), 3 hunki, `FIRMWARE_VERSION` **v3.56.4**.
Zero zmian w `HistoriaPobranie.h`, w panelu i w układzie include'ów.

SHA-256 pliku po patchu: `f89874f3390009acd79632db1031d04cd90e67435eda68e2007ea7c4b3168e38`
(46995 linii).

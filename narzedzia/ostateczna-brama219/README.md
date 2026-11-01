# Etap 219 — ostateczna brama pliku, który wgrasz

Ten etap **nie zmienia firmware**. Bierze plik `main_centrala.cpp` po wszystkich
łatkach (v3.56.4 z etapu 218) i sprawdza całość 16 kontrolami, zanim trafi on
na Twoje PlatformIO. Ma odpowiedzieć na jedno pytanie: *czy w tym pliku nie ma
już niczego rozjechanego?*

## Co jest sprawdzane

| Kod | Kontrola |
|---|---|
| B01 | kodowanie UTF-8, brak znaku NUL, brak końców linii CRLF |
| B02 | nawiasy `{}` `()` `[]` zbalansowane w **całym** pliku (lekser pomija komentarze, stringi i raw stringi) |
| B03 | dokładnie jeden raw string strony (`R"PIECHTML(…)"`) z `<!DOCTYPE html>` i `</html>` |
| B04 | panel (`id="histBtn"`) jest **wewnątrz** tego raw stringa, a nie obok |
| B05 | strona zaczyna się DOCTYPE, kończy `</html>` |
| B06 | wszystkie tagi HTML zbalansowane (285 `div`, 93 `span`, 44 `button`…) |
| B07 | bloki `<script>` policzone i domknięte |
| B08 | składnia **każdego** bloku JavaScript (`node --check`) |
| B09 | `ESPAsyncWebServer.h` **przed** `HistoriaPobranie.h` / `HistoriaBilety.h` (błąd D1 z etapu 214) |
| B10 | każdy z czterech include'ów dokładnie raz |
| B11 | `FIRMWARE_VERSION` dokładnie jedna definicja = `v3.56.4` |
| B12 | każda nowa zmienna globalna zdefiniowana dokładnie raz |
| B13 | wszystkie trzy trasy `/history*` przez `exact()` |
| B14 | **żadna** z 46 tras nie jest martwa (przechwycona przez wcześniejszą) |
| B15 | brak `TODO` / `FIXME` / `XXX` / znaczników konfliktu w kodzie |
| B16 | matryca poprawek 211–218: każda jest w pliku **i** każdej nie ma w bazie v3.55.8 |

Matryca B16 jest po to, żeby kontrola nie była pusta: sam „obecny znacznik”
nic nie dowodzi, jeśli ten znacznik był tam od zawsze. Dlatego każdy znacznik
jest liczony także w pliku bazowym v3.55.8.

## Ten sam audyt na trzech stanach

Audyt puszczono na trzech plikach zbudowanych od zera (SHA-256 sprawdzany):

| Stan | Wynik | Czerwone kontrole | Dlaczego tak |
|---|---|---|---|
| v3.55.8 (baza) | **czerwony** | B04, B09, B10, B11, B12, B13, B16 | brak wszystkich poprawek i panelu |
| v3.56.3 (przed 218) | **czerwony** | B11, B13, B14, B16 | trasy `/history*` na gołym napisie → 2 martwe trasy |
| v3.56.4 (do wgrania) | **zielony** | — | 16/16 |

To jest ważniejsze niż sam zielony wynik: narzędzie **odróżnia** stan popsuty
od naprawionego i wskazuje dokładnie te kontrole, które odpowiadają znanym
błędom (D1 z 214, przechwytywanie tras z 218).

## Mutanty (10/10 czerwonych)

| Mutant | Co psuje | Czerwone |
|---|---|---|
| M1 | urwany jeden `}` | B02 |
| M2 | raw string bez zamykającego cudzysłowu | B02, B03, B04, B05, B06, B07, B08, B16 |
| M3 | panel bez `id="histBtn"` | B04 |
| M4 | `HistoriaPobranie.h` przed `ESPAsyncWebServer.h` (błąd D1) | B09 |
| M5 | wersja cofnięta na v3.56.3 | B11 |
| M6 | `/history/long` na gołym napisie | B13, B16 |
| M7 | błąd składni JS w panelu | B08, B16 |
| M8 | nadmiarowy `</div>` w stronie | B06 |
| M9 | druga definicja `historiaBramka` | B12 |
| M10 | końce linii CRLF | B01 |

## Wynik

```
PASS audyt219 v3.56.4: 16 kontroli, wszystkie zielone
PASS audyt219 v3.56.3: czerwony na B11, B13, B14, B16 (oczekiwane)
PASS audyt219 v3.55.8: czerwony na B04, B09, B10, B11, B12, B13, B16 (oczekiwane)
PASS brama219 stanow=3 mutantow=10 kontroli_na_stan=16
```

Plik do wgrania: **2 376 655 bajtów**, 46 995 linii, strona 503 330 bajtów,
5 raw stringów (strona = `PIECHTML`), 46 tras, 0 martwych.

## Czego ta brama NIE dowodzi

- że PlatformIO skompiluje plik (kompilator ESP jest tylko u Ciebie),
- że kod jest poprawny semantycznie — nawiasy mogą się zgadzać, a logika być zła
  (to sprawdzają etapy 211–218 na prawdziwych fragmentach),
- że strona dobrze wygląda w telefonie (sprawdzana jest budowa i składnia, nie CSS),
- że `OnStep`/WiFi/LittleFS zachowają się jak w modelu.

Brama B02 (nawiasy) działa na własnym lekserze: pomija `//`, `/* */`, stringi,
znaki i raw stringi — dzięki temu np. `"{"` w tekście strony nie jest liczony
jako nawias kodu.

## Uruchomienie

```
python3 -B narzedzia/ostateczna-brama219/run.py
```

Pojedynczo:

```
python3 -B narzedzia/ostateczna-brama219/audyt.py main_centrala.cpp \
    --baza firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp \
    --wersja v3.56.4 --wynik audyt.json
```

## Dla użytkownika

Ten etap **nic nie zmienia** — plik do wgrania jest ten sam co z etapu 218
(sha256 `f89874f3390009acd79632db1031d04cd90e67435eda68e2007ea7c4b3168e38`).
Polecenia pozostają te same:

```
patch -p1 -i main_centrala-v3.55.8-to-v3.56.4.patch    # z etapu 218
cp narzedzia/od-przycisku216/HistoriaPobranie.h .
```

Potem PlatformIO. Po wgraniu: telefon → „⬇ HISTORIA CSV” → „Krótka (24 h)”
zapisuje `historia-krotka.csv`, „Długa (archiwum)” — `historia-dluga.csv`.

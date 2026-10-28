# Etap 214 — brama przed wgraniem i poprawka kolejności include (v3.56.2)

**Status: KANDYDAT. NIE WGRYWAĆ.** Ten etap nic nie wgrywa. Daje jeden patch,
który **dopiero teraz ma szansę się skompilować**, i bramę, która takie błędy
łapie bez kompilatora ESP.

## 1. Co się stało (najważniejsze)

Podczas przygotowania tego etapu okazało się, że **etapy 212 i 213 nie
skompilują się na ESP**. W pliku po patchu wyglądało to tak:

```
5139: #include "HistoriaPin.h"
5140: #include "HistoriaPobranie.h"     <-- trasa, która DZIEDZICZY po AsyncAbstractResponse
5141: #include "HistoriaBilety.h"
...
5227: #include <ESPAsyncWebServer.h>    <-- a klasa AsyncAbstractResponse jest dopiero tutaj
```

`HistoriaPobranie.h` deklaruje `class Odpowiedz final : public AsyncAbstractResponse`
i woła `request->method()`, `request->getHeader()`, `request->send()`.
W linii 5140 kompilator nie zna jeszcze **żadnej** z tych nazw.

**Dlaczego testy tego nie zauważyły:** każdy `Test.cpp` wciąga najpierw model
serwera (`Serwer212.h` / `Serwer213.h`), a dopiero potem nagłówek trasy.
W teście kolejność jest poprawna, w firmware — nie. Test hostowy z definicji
nie widzi kolejności `#include` w pliku, którego nie kompiluje.

**Poprawka (v3.56.2):** oba nagłówki przeniesione **za** `#include <ESPAsyncWebServer.h>`.
Poza dwiema liniami i numerem wersji nic się nie zmienia.

## 2. Czego używać

| Twoja sytuacja | Plik |
|---|---|
| masz czyste v3.55.8 | `main_centrala-v3.55.8-to-v3.56.2.patch` (211+212+213+214) |
| masz już v3.56.1 (etap 213) | `main_centrala-v3.56.1-to-v3.56.2.patch` |
| masz v3.55.9 / v3.56.0 | najpierw dojdź do 3.56.1, potem mały patch — albo od razu skumulowany na czyste 3.55.8 |

Patche z katalogów `pobranie-trasa212/` i `panel-pobrania213/` są **przestarzałe**
(mają ten błąd) — leżą tam jako zapis historyczny, z plikiem
`UWAGA-PRZESTARZALE.md`.

## 3. Brama: 18 kontroli (S01–S18)

Brama czyta plik **po patchu** i sprawdza rzeczy, których test hostowy nie widzi:

| # | kontrola | po co |
|---|---|---|
| S01 | `FIRMWARE_VERSION = "v3.56.2"` dokładnie raz | wersja podniesiona |
| S02 | każdy z 3 include'ów etapów 211–213 występuje raz | brak zdublowania |
| S03 | **nagłówki HTTP PO `<ESPAsyncWebServer.h>`** | **ten błąd z 212/213** |
| S04 | 8 nowych symboli globalnych ma po 1 definicji | brak kolizji i dubli |
| S05 | domyślny token pusty | pobieranie wyłączone bez `secrets.h` |
| S06 | `secrets.h.example`: placeholder, nie sekret | brak wycieku |
| S07 | w repo nie ma `secrets.h` | brak wycieku |
| S08 | `/history` ANY, `/history/long` ANY, `/history/link` GET | metody tras |
| S09 | przycisk panelu w literalzie HTML, bez tokenu, z biletem | brak rozbicia stringa i brak sekretu w HTML |
| S10 | patch jest odwracalny (`patch -R --dry-run`) | plik jest dokładnie tym, co opisuje patch |
| S11 | bez CRLF i BOM | nie psuje diffów |
| S12 | nagłówki firmware bez STL (tylko 6 dozwolonych include'ów) | ESP nie lubi balastu |
| S13 | bez `throw`, `dynamic_cast`, `std::function` | brak wyjątków/RTTI na ESP |
| S14 | oba wysyłacze Telegramu nadal na zapince 211 | 211 nietknięte |
| S15 | `historiaSys` zdefiniowany przed bramką | kolejność definicji |
| S16 | `esp_system.h` przed pierwszym **użyciem** `esp_random()` (komentarze nie liczą się) | losowość biletów |
| S17 | nagłówki kompilują się jako **gnu++11** i gnu++17 | ESP32 Arduino domyślnie C++11 |
| S18 | sekcja tokenu dokładnie jedna | brak sklejek przy patchowaniu |

## 4. Po czym poznać, że brama nie kłamie

`selftest.py` — 16 przypadków. Najważniejszy: brama puszczona na **pliku
z etapu 213** (v3.56.1) **zatrzymuje go** (S03). Poza tym 11 celowych zepsuć:

| zepsucie | łapie |
|---|---|
| include z powrotem przed serwer | S03 |
| wersja nie podniesiona | S01 |
| dwie definicje `HISTORIA_TOKEN` | S04 |
| domyślny token `"abc"` | S05 |
| `/history/link` jako POST | S08 |
| `)PIECHTML` w środku panelu | S09 |
| stary token wklejony do panelu | S09 |
| prawdziwy sekret w `secrets.h.example` | S06 |
| `#include <string>` w nagłówku | S12 |
| `throw` w nagłówku | S13 |
| brak `pin.close()` w wysyłaczu | S14 |
| brak `esp_system.h` | S16 |
| CRLF w pliku | S11 |

## 5. Co dalej zrobić (Ty, nie ja)

```bash
cd <katalog z main_centrala.cpp, HistoriaPin.h, HistoriaPobranie.h, HistoriaBilety.h>
patch -p1 -i main_centrala-v3.55.8-to-v3.56.2.patch
grep -n 'FIRMWARE_VERSION = "v3.56.2"' main_centrala.cpp        # linia 5591
grep -n -A2 '<ESPAsyncWebServer.h>' main_centrala.cpp | head -5  # za nim dwa nasze include'y
```

Potem kompilacja ESP (PlatformIO / arduino-cli — jak zwykle u Ciebie).
**To jest pierwszy moment, w którym ten kod zobaczy prawdziwy kompilator.**
Jeśli coś nie zadziała, wyślij mi pierwszą linię błędu — poprawię.

## 6. Czego ten etap NIE robi

* nie wgrywa nic na sterownik, nie dotyka NVS, OTA, CSV, rotacji, rytmu 8 s / 60 s,
* nie zmienia zachowania trasy ani biletów (tylko kolejność include'ów),
* nie jest dowodem działania na urządzeniu — nadal brakuje kompilacji ESP,
  testu na sprzęcie i Twojego wgrania.

## 7. Pliki

| Plik | Rola |
|---|---|
| `main_centrala-v3.55.8-to-v3.56.2.patch` | **patch do użycia** (211+212+213+214) |
| `main_centrala-v3.56.1-to-v3.56.2.patch` | mały patch, gdy masz już 3.56.1 |
| `zrob_patch.py` | generuje oba patche (bez ręcznego pisania diffów) |
| `sprawdz.py` | brama, 18 kontroli |
| `selftest.py` | 16 przypadków dowodzących, że brama łapie błędy |
| `build.py` | patche + odwracalność + brama + `RAPORT.md` |
| `run.py` | build + selftest → `WYNIKI/runNNN/wynik.json` |
| `RAPORT.md`, `KONTROLA.json` | wynik bramy z ostatniego uruchomienia |
| `PLAN.md`, `AUDYT.json` | kolejka i twierdzenia sprawdzane maszynowo |

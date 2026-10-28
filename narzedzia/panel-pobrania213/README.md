# Etap 213 — przycisk „Pobierz historię” w panelu (patch v3.56.1)

**Status: KANDYDAT. NIE WGRYWAĆ na sterownik.** Ten etap nic nie wgrywa —
produkuje patch `main_centrala.cpp` i testy hostowe, które ten patch sprawdzają.

## 1. Po co to jest

Po etapie 212 historię można było pobrać tylko komendą z tokenem:

```bash
curl -H "Authorization: Bearer <token>" http://<adres>/history -o hist.csv
```

To działa, ale nie da się tego zrobić z telefonu „na dziś”. Etap 213 dodaje w panelu
WWW przycisk **⭳ HISTORIA CSV** i dwa przyciski: krótka (24 h) / długa (archiwum).

Najważniejsze: **staly token z `secrets.h` nie trafia do strony.** Przycisk prosi
centralę o **bilet** — 32 znaki hex, ważny 5 minut, jednorazowy — i dopiero z nim
otwiera `/history?k=<bilet>`.

## 2. Jak to działa krok po kroku

1. Klikasz **⭳ HISTORIA CSV** → wysuwa się mały panel (nie opuszcza strony).
2. Klikasz „Krótka (24 h)” → przeglądarka woła `GET /history/link?stream=short`.
3. Centrala sprawdza, że token jest ustawiony w `secrets.h`, losuje 16 bajtów
   (`esp_random()`), zapisuje bilet w jednym z 4 miejsc i zwraca 32 znaki hex
   z nagłówkiem `Cache-Control: no-store`.
4. Przeglądarka otwiera `/history?k=<bilet>` — trasa z etapu 212 sprawdza bilet,
   **spala go** i wysyła plik.
5. Drugie użycie tego samego linku → 401. Po 5 minutach → 401. Link wysłany
   komuś na telefon działa **raz**.

Bilet jest przypięty do strumienia: bilet na „długą” nie otworzy „krótkiej”.
Cztery bilety mogą czekać naraz; piąty unieważnia ten, który wygasa najwcześniej.

## 3. Co jest sprawdzone i czym

* **20 grup / 126 sprawdzeń** (etap 213) w trzech profilach kompilacji
  (`-O2 -Werror`, ASan+UBSan+LSan, `-D_GLIBCXX_ASSERTIONS`) — wszystkie PASS.
* **Regresja etapu 212:** cały zestaw 212 (20 grup / 132 sprawdzenia) skompilowany
  przeciwko **nowemu** nagłówkowi `HistoriaPobranie.h`. Nadal PASS — 213 nie psuje
  trasy 212.
* Testy uruchamiają **prawdziwy tekst z pliku po patchu**: bramkę i obiekt trasy
  (`Extracted212.inc`), adaptery `historiaLosuj213`/`historiaZegar213`, trzy
  rejestracje `wifiWebServer.on(...)` i tekst panelu (`Extracted213.inc`).
  Modelowane jest tylko to, czego nie ma na komputerze: serwer HTTP, VFS w RAM,
  semafory, `esp_random`.
* **12 mutantów, wszystkie wykryte** — m.in. bilet wielokrotny, bilet na każdy
  strumień, brak terminu ważności, bilet bez sprawdzenia długości, wypychanie
  najnowszego, brak sprawdzenia metody, brak nagłówka `Cache-Control`,
  stały token wklejony do panelu.
* **Powtarzalność:** `build.py` regeneruje `HistoriaPobranie.h` i `host/Serwer213.h`
  z plików etapu 212 i porównuje **bajt w bajt** z tym, co jest w paczce.
  Patch skumulowany (3.55.8 → 3.56.1) i łańcuch 211 + 212 + 213 dają identyczny plik.

Sprawdzenie „po czym poznać, że działa” bez wgrywania: test 17 czyta tekst panelu
i wymaga, żeby **nie było w nim** `HISTORIA_TOKEN`, `SECRET_HISTORIA_TOKEN` ani
`Bearer `, a było `'/history/link?stream='`, `encodeURIComponent` i sprawdzenie
`length !== 32`.

## 4. Pliki tego etapu

| Plik | Rola |
|---|---|
| `HistoriaBilety.h` | magazyn biletów (4 miejsca, 5 min, jednorazowe, bez alokacji) |
| `HistoriaPobranie.h` | trasa 212 + obsługa `?k=` + `link()` |
| `main_centrala-v3.56.0-to-v3.56.1.patch` | patch na plik po etapie 212 |
| `main_centrala-v3.55.8-to-v3.56.1.patch` | patch skumulowany (211+212+213) na czyste 3.55.8 |
| `zrob_patch.py` | generuje patch **i** oba pliki pochodne z plików 212 |
| `build.py` | weryfikacja powtarzalności, ekstrakcja prawdziwych fragmentów |
| `Extracted213.inc` | wygenerowany: adaptery, rejestracje, tekst panelu |
| `host/Serwer213.h` | model serwera (212 + parametry zapytania) |
| `host/Model213.h` | `esp_random()` i zegar do testów |
| `Test.cpp` | 20 grup |
| `run.py` | 3 profile × (213 + regresja 212) |
| `mutacje.py` | 12 mutantów |
| `PLAN.md`, `AUDYT.json` | kolejka etapów i twierdzenia sprawdzane maszynowo |

## 5. Gotowa komenda (gdybyś wgrywał)

```bash
cd <katalog z main_centrala.cpp>
# masz już 3.56.0 (etap 212):
patch -p1 -i main_centrala-v3.56.0-to-v3.56.1.patch
# albo z czystego 3.55.8, jednym patchem (zawiera 211+212+213):
patch -p1 -i main_centrala-v3.55.8-to-v3.56.1.patch
grep -n 'FIRMWARE_VERSION = "v3.56.1"' main_centrala.cpp     # linia 5584
```

Do katalogu z `main_centrala.cpp` muszą trafić **trzy** pliki nagłówkowe:
`HistoriaPin.h` (211), `HistoriaPobranie.h` (213) i `HistoriaBilety.h` (213).

## 6. Znane ograniczenia (świadome)

1. **`/history/link` nie ma uwierzytelniania.** Kto dosięgnie panelu, dostanie
   bilet. To nie jest regresja: `/api/history` już dziś oddaje historię bez
   żadnego tokenu. Zysk jest węższy, ale prawdziwy: stały token nie wycieka do
   HTML-a, a link sam się spala.
2. **Bilet spala się także przy 404/413/500** — pobranie się nie zaczęło, ale
   bilet już nie działa. Leczysz to jednym kliknięciem. Świadomie wybrane:
   prostsze i bezpieczniejsze niż trzymanie biletu po nieudanej próbie.
3. **Jedno pobranie naraz** (odziedziczone po 212) — drugie dostaje 503.
4. **Brak buildu ESP i testu na urządzeniu** — to etap offline.

## 7. Komendy

```bash
python3 -B narzedzia/panel-pobrania213/zrob_patch.py
python3 -B narzedzia/panel-pobrania213/build.py
python3 -B narzedzia/panel-pobrania213/run.py
python3 -B narzedzia/panel-pobrania213/mutacje.py
```

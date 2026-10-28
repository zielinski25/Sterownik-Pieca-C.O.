# Etap 212 — pobranie historii przez HTTP (`/history`, `/history/long`)

**Status: KANDYDAT. NIE WGRYWAĆ na sterownik.** Ten etap nic nie wgrywa i niczego
nie zmienia na urządzeniu — produkuje wyłącznie patch do pliku `main_centrala.cpp`
i testy hostowe, które ten patch sprawdzają.

## 1. Co ten etap dodaje

Dwa nowe adresy HTTP w centrali:

| Adres | Co zwraca | Plik, z którego czyta |
|---|---|---|
| `/history` | `text/csv`, historia krótka (24 h) | `/littlefs/hist_short.csv` |
| `/history/long` | `text/csv`, historia długa | `/littlefs/hist_long.csv` |

Oba wymagają nagłówka `Authorization: Bearer <token>`; token pochodzi z
`secrets.h` (`SECRET_HISTORIA_TOKEN`). **Bez wpisu w `secrets.h` oba adresy są
zamknięte** (503) — stara kopia `secrets.h` niczego nie otwiera.

Odpowiedzi odmowy (prawdziwe odpowiedzi HTTP, nie zerwanie połączenia):

| Sytuacja | Kod |
|---|---|
| metoda inna niż GET | 405 + `Allow: GET` |
| brak / zły token | 401 + `WWW-Authenticate: Bearer` |
| token nie ustawiony w `secrets.h` | 503 |
| trwa inne pobranie | 503 |
| magazyn historii zatrzymany / brak LittleFS | 503 |
| plik nie istnieje albo brak potwierdzonego prefiksu | 404 |
| prefiks większy niż 50 MB | 413 |
| VFS odmówił otwarcia | 500 |

## 2. Dlaczego to jest bezpieczniejsze niż „pobierz plik”

Trasa nie czyta pliku „na teraz” i nie ogłasza długości z pamięci. Używa tej samej
zapinki co wysyłacze Telegramu z etapu 211 (`historia211::Pin`):

1. bierze `logMutex` — tę samą blokadę, którą bierze zapis historii,
2. otwiera plik **raz** i ustala długość = `min(znacznik potwierdzenia, rozmiar pliku)`,
3. **oddaje blokadę natychmiast** — nigdy nie trzyma jej przez czas wysyłki,
4. wysyła dokładnie tyle bajtów, ile ogłosiła w `Content-Length`,
5. każdy krótszy odczyt lub błąd odczytu **zrywa połączenie** i unieważnia
   odpowiedź; nie ma sytuacji „wysłaliśmy 200, a w środku brakuje kilobajta”.

Znacznik potwierdzenia (`g_histShortSize` / `g_histLongSize`) jest aktualizowany
dopiero po udanym zapisie i weryfikacji. Dlatego `min(...)` oznacza: wysyłamy
tylko ten prefiks, o którym wiemy, że jest **i** zapisany, **i** potwierdzony.

## 3. Jak to włączyć (gdy zdecydujesz się wgrywać)

1. W `secrets.h` dopisz jedną linię (wzór jest w `secrets.h.example`, sekcja 5):

   ```c
   #define SECRET_HISTORIA_TOKEN   "twoj-wlasny-dlugi-losowy-token"
   ```

   Token: minimum 32 znaki, inny niż `SECRET_CMD_TOKEN`. Nie wklejaj go do gita,
   na czat ani do zgłoszenia.

2. Nałóż patch (albo od razu skumulowany, jeśli **nie** masz jeszcze etapu 211):

   ```bash
   # w katalogu z main_centrala.cpp i secrets.h.example
   patch -p1 -i main_centrala-v3.55.9-to-v3.56.0.patch     # masz już 211
   # albo
   patch -p1 -i main_centrala-v3.55.8-to-v3.56.0.patch     # masz czyste 3.55.8
   ```

3. Sprawdź, że w pliku jest `static const char* FIRMWARE_VERSION = "v3.56.0";`
   (linia 5577 po patchu skumulowanym).
4. Wgraj jak zwykle — **to Ty wgrywasz**, nie ja.
5. Test po wgraniu: `curl -H "Authorization: Bearer <token>" http://<adres>/history -o hist.csv`
   i porównaj rozmiar z `/api/history` (albo z licznikiem w UI).

## 4. Co jest sprawdzone i czym

* **20 grup / 132 sprawdzenia** w trzech profilach kompilacji
  (`-O2 -Werror`, ASan+UBSan+LSan, `-D_GLIBCXX_ASSERTIONS`) — wszystkie PASS.
  Testy uruchamiają **prawdziwy tekst funkcji** wycięty z `main_centrala.cpp`
  po patchu (`Extracted212.inc`), a nie kopię napisaną na potrzeby testu.
* **12 mutantów** — każdy celowo psuje jedną obietnicę trasy i każdy jest
  wykrywany (`MUTACJE.json`). M.in.: brak sprawdzenia metody, brak sprawdzenia
  tokenu, akceptacja prefiksu tokenu, nieustawiana blokada „zajęte”,
  `Content-Length` o 1 za duży, krótki odczyt potraktowany jako koniec,
  źródło zawsze „ważne”, blokada niezwalniana, brak `logMutex` w bramce,
  magazyn zawsze „gotowy”, 413 jako 200, 405 jako 200.
* **Powtarzalność patcha** — `build.py` nakłada patch skumulowany
  (3.55.8 → 3.56.0) i łańcuch 211 + 212; oba dają **bajt w bajt** ten sam plik.
  Patch 3.55.9 → 3.56.0 nie dotyka niczego, co wprowadził etap 211.

## 5. Znane ograniczenia (świadome, opisane — nie „naprawione po cichu”)

1. **Plik istnieje, ale ma 0 B** → zapinka z etapu 211 klasyfikuje to jako
   `Fault`, więc trasa odpowiada **500**, a nie 404. Test 09 sprawdza właśnie
   500. Chcesz 404 — trzeba zmienić etap 211, nie ten.
2. **VFS odmawia otwarcia z powodu innego niż brak pliku** (np. błąd I/O) →
   zapinka zwraca `Missing`, więc leci **404** zamiast 500. Etap 211 nie
   rozróżnia `ENOENT` od `EIO` przy `open()`.
3. **Tabulator po `Bearer`** jest odrzucany (401). RFC 7235 dopuszcza tam
   wyłącznie spację — to celowa surowość, nie przeoczenie.
4. Trasa jest **jedną na raz**: drugie pobranie w trakcie pierwszego dostaje
   503. To ograniczenie pamięci, nie błąd — ESP32 nie utrzyma dwóch strumieni
   po 50 MB.
5. Nadal **brak buildu ESP i braku testu na urządzeniu** — to etap offline.

## 6. Pliki tego etapu

| Plik | Rola |
|---|---|
| `HistoriaPobranie.h` | cała trasa: metoda, token, zajętość, magazyn, odpowiedź strumieniowa |
| `main_centrala-v3.55.8-to-v3.56.0.patch` | patch skumulowany (211 + 212) na czyste 3.55.8 |
| `main_centrala-v3.55.9-to-v3.56.0.patch` | patch tylko 212, na plik po etapie 211 |
| `zrob_patch.py` | generuje oba patche z przypiętego pliku (SHA sprawdzany) |
| `build.py` | nakłada patche, porównuje oba warianty, wyciąga prawdziwy tekst bramki |
| `Extracted212.inc` | wygenerowany tekst bramki i obiektów trasy z pliku po patchu |
| `host/Serwer212.h` | model serwera HTTP (tylko to, czego dotyka trasa) |
| `Test.cpp` | 20 grup testów |
| `run.py` | trzy profile kompilacji → `WYNIKI/runNNN/wynik.json` |
| `mutacje.py` | 12 mutantów → `MUTACJE.json` |
| `AUDYT.json` | twierdzenia sprawdzane maszynowo |
| `PLAN.md` | miejsce etapu 212 w kolejce 207 → 214 |

## 7. Komendy

```bash
python3 -B narzedzia/pobranie-trasa212/zrob_patch.py   # regeneracja patchy
python3 -B narzedzia/pobranie-trasa212/build.py        # weryfikacja + ekstrakcja
python3 -B narzedzia/pobranie-trasa212/run.py          # testy, 3 profile
python3 -B narzedzia/pobranie-trasa212/mutacje.py      # mutacje
```

## 8. Czego ten etap NIE robi

* nie wgrywa niczego na sterownik i nie dotyka NVS, OTA, sterowania, CSV,
  rotacji historii ani rytmu 8 s / 60 s,
* nie zmienia formatu dokumentów Telegramu ani `/api/history`,
* nie zawiera żadnego sekretu — w repo jest wyłącznie zastepnik
  `ZMIEN-MNIE-NA-DLUGI-LOSOWY-TOKEN-32-ZNAKI`,
* nie jest „gotowy do wdrożenia”: `release_ready = false`, bo brak buildu ESP
  i testu na sprzęcie.

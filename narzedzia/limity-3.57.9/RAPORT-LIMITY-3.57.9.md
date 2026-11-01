# Symulacje limitów — 3.57.8 → co musi naprawić 3.57.9

Data: 2026-11-01 · baza: **3.57.8** · katalog: `narzedzia/limity-3.57.9/`
Wynik: **38/38** scenariuszy C++ (prawdziwy `history193::Engine`) + **19/19** kontroli zgodności ze źródłem.

Kolejność pracy (ustalona): **1. symulacje limitów → 2. poprawki → 3. paczka.**
Ten dokument zamyka punkt 1. Firmware nie był zmieniany.

---

## 1. Odpowiedź w trzech zdaniach

Historia rotuje się poprawnie co do jednej linii, ale **rotacja potrzebuje chwilowo
dwóch kopii pliku** (2,97 MB dla długiej historii, a firmware rezerwuje na nią 2 MB),
a **nieudana rotacja zostawia plik `.rebuild`, którego nic nigdy nie usuwa** — po nim
każda następna rotacja, także po restarcie, kończy się błędem, historia staje
na zawsze, a pliku nie da się usunąć z panelu (409, ścieżka chroniona).
Do tego jeden błąd I/O zatrzymuje **obie** historie naraz i nie ma odblokowania.

---

## 2. Co dokładnie było symulowane

| element | jak |
|---|---|
| zapis i rotacja | **prawdziwa klasa** `history193::Engine` z `CheckedHistoryIO.h` 3.57.8 (SHA-256 `8eb18693…`), bez żadnych zmian w kodzie |
| dysk | model o skończonej pojemności: zajęcie po blokach 4 KB jak w LittleFS, `ENOSPC`, `rename()` podmienia cel, `O_EXCL` → `EEXIST` |
| warstwa wyżej | 1:1 z firmware: append, potem trim **tylko gdy** `linie > limit + margines` |
| rozmiar linii | **zmierzony** z prawdziwych plików centrali: krótka **71 B**, długa **180 B** |
| progi | 1440/1000 (krótka) i 8000/500 (długa), kolejka 4096 B, `FS_RESERVE_HIST_LONG_B` 2 MB |

Scenariusze: L1 rotacja długa, L2 minimalna pojemność dysku, L3 brak miejsca
w trakcie rotacji, L3b stary `.rebuild` na dysku, L4 dysk zapchany logami,
L4b urwany ogon linii, L5/L6 granice 2440/2441 i 8500/8501, L7 kolejka RAM, L8 obie historie naraz.

---

## 3. Co potwierdzone (działa)

| sprawdzenie | wynik |
|---|---|
| rotacja krótkiej historii: przy 2440 liniach **nie**, przy 2441 **tak** | bez off-by-one, po rotacji dokładnie 1440 linii |
| rotacja długiej historii: przy 8500 **nie**, przy 8501 **tak** | po rotacji dokładnie 8000 linii |
| plik po rotacji | kompletny, `rename()` atomowo podmienia cel, brak śmieci |
| nieudana rotacja | **nie kasuje i nie zeruje** pliku historii (dane całe) |
| kolejka RAM 4096 B | mieści **57** próbek krótkich (~57 min) lub **22** długich |
| odrzucenie próbki z pełnej kolejki | jest logowane `WARN` („nowa próbka nieprzyjęta…”), nie ciche |
| klucze NVS | 71 kluczy, wszystkie ≤ 15 znaków (poprawka z 3.57.8 trzyma) |

---

## 4. Pięć znalezisk

### F1 — rotacja potrzebuje 2× pliku, a rezerwa mówi o jednej kopii

Rotacja nie nadpisuje pliku w miejscu: powstaje obok kopia robocza
(`.rebuild`), a `rename()` podmienia ją na oryginał. W chwili szczytu
na dysku są **oba** pliki.

| historia | plik przed rotacją | kopia robocza | **szczyt** |
|---|---|---|---|
| krótka (2441 linii) | 173 311 B | 102 240 B | **278 528 B** (po blokach) |
| długa (8501 linii) | 1 530 180 B | 1 440 000 B | **2 973 696 B** (po blokach) |
| najgorszy przypadek: rotacja długiej + pełna krótka | — | — | **3 149 824 B ≈ 3,00 MB** |

Symulacja znalazła dokładnie to samo: najmniejsza pojemność, przy której
8600 próbek długich przechodzi, to **2,84 MB** — sama historia długa.

Firmware deklaruje `FS_RESERVE_HIST_LONG_B = 2 097 152 B` (2 MB) — czyli
**o ~0,88 MB za mało** na samą rotację długiej historii, a cała rezerwa
trybu SHADOW (coredump 1,25 MB + krótka 128 kB + długa 2 MB + krytyczne
310 641 B = **3,67 MB**) nie pokrywa nawet najgorszego przypadku 3,00 MB
razem z logami. Ta rezerwa i tak **niczego nie egzekwuje** — jest tylko diagnostyką.

### F2 — nieudana rotacja zostawia `.rebuild`, który blokuje rotację na zawsze

`Engine::trim()` przy błędzie zamyka plik roboczy, ale **go nie usuwa**.
W repo nie ma ani jednego miejsca, które kasuje `hist_*.csv.rebuild`,
a panel blokuje te ścieżki (409 „Chroniona historia/plik przejścia”).

Skutek zmierzony w symulacji (L3b): jeśli na dysku leży stary `.rebuild`
— np. po zaniku prądu w trakcie rotacji albo po `ENOSPC` — to:

1. pierwsza rotacja kończy się `Fault::Open` (`O_CREAT|O_EXCL` na istniejącym pliku),
2. `Engine` blokuje się **na stałe** (zatrzask błędu),
3. **restart nie pomaga** — plik zostaje na dysku, więc kolejna rotacja
   kończy się tym samym błędem,
4. usunąć go zdalnie nie można (panel: 409; edytor plików: ta sama lista).

To jest jedyna z pięciu rzeczy, która wymaga **fizycznego dostępu do centrali**
(USB/serial), a wyzwala ją zwykły brak miejsca.

### F3 — jeden błąd zatrzymuje obie historie i nic ich nie odblokowuje

W firmware jest **jeden** `history193::Engine` (`historiaIo`) dla obu strumieni.
Zatrzask `fault_` jest wspólny: po błędzie zapisu krótkiej historii
przestaje działać też długa i odwrotnie — potwierdzone (L3, L4).

Nie ma żadnej ścieżki odblokowania: 100 kolejnych prób po błędzie kończy się
tak samo. Restart pomaga **tylko wtedy**, gdy przyczyna zniknęła
(np. logi zwolniły miejsce) — przy F2 nie znika.

### F4 — urwany ogon linii to trwała blokada historii

Kontrola granicy w `Engine::append()` wymaga, żeby plik kończył się znakiem `\n`.
Jeśli ogon jest urwany (uszkodzony zapis), każdy kolejny append kończy się
`Fault::Boundary`, a przy starcie **nic tego nie naprawia** — `historiaWczytajZFS()`
tylko czyta, nie poprawia pliku. Restart nie pomaga, dane na dysku zostają,
ale nowe próbki już nie dochodzą (L4b).

### F5 — brak miejsca to nie „chwilowy problem”, tylko koniec historii

Jeden `ENOSPC` (np. gdy logi zajmą wszystko) blokuje zapis obu historii
do restartu (L4). Nie ma żadnej rezerwy wymuszanej przed zapisem —
`fsQuotaShadowEvaluate()` tylko raportuje.

---

## 5. Jak to się ma do realnego układu (do potwierdzenia na centrali)

Ile miejsca ma Twoja partycja LittleFS, nie wynika z repo (brak `partitions.csv`).
Wystarczy jedno spojrzenie w panel: `/api/fs-list` podaje `used`/`total`.
Poniżej progi, które można porównać z tą liczbą:

| wymóg | wartość |
|---|---|
| wolne miejsce potrzebne **w chwili rotacji długiej historii** | ≈ **3,0 MB** |
| wolne miejsce potrzebne **w chwili rotacji krótkiej** | ≈ **0,28 MB** |
| logi mogą rosnąć do 79 % pojemności (`LOG_FS_ROTATE_PERCENT`) | przy 4 MB → 3,31 MB, przy 10,4 MB → 8,61 MB |

Logi dozwolone do 79 % + historia do 3,0 MB to razem **ponad 100 %** każdej
pojemności — przy 4 MB kolizja jest pewna, przy 10,4 MB też (8,61 + 3,00 = 11,6 MB).
Jedynym, co ratuje sytuację, jest backstop logów (`<20 % wolnego` → przycięcie
logów do 256 kB) — ale on nie rezerwuje miejsca dla rotacji historii, więc
okno na `ENOSPC` (i na F2) jest realne.

Ile to trwa w praktyce: krótka historia dobija do progu rotacji po
**2441 próbkach ≈ 40,7 h ≈ 1,7 doby** (próbka co 60 s). Długa — po
8501 zdarzeniach (heartbeat 60 min), czyli rzędu roku.

---

## 6. Co proponuję zrobić w 3.57.9 (do Twojej zgody, w tej kolejności)

| # | poprawka | zamyka | ryzyko zmiany |
|---|---|---|---|
| 1 | przy starcie: usunąć `hist_short.csv.rebuild` i `hist_long.csv.rebuild` **oraz** obciąć urwany ogon do ostatniego `\n` (przed `historiaWczytajZFS()`) | F2, F4 | małe — dotyka tylko plików roboczych i ogona |
| 2 | `Engine::trim()`: przed `O_EXCL` spróbować usunąć stary plik roboczy (albo zamiast `O_EXCL` użyć `O_TRUNC`) | F2 | małe, ale zmienia warstwę audytowaną — do decyzji |
| 3 | przed rotacją sprawdzić wolne miejsce (potrzeba ≈ rozmiar pliku); brak miejsca = **odłóż rotację**, nie blokuj zapisu | F5, F1 | średnie — zmienia politykę |
| 4 | odblokowanie po czasie: po N minutach bez zmian spróbować ponownie zamiast trwałego zatrzasku | F3 | średnie |
| 5 | `FS_RESERVE_HIST_LONG_B` 2 MB → 3 MB i **egzekwowanie** rezerwy zamiast trybu SHADOW | F1 | wymaga decyzji, bo dotyka logów |

Punkty 1 i 3 są niezależne od siebie i najmniej ryzykowne — od nich bym zaczął.
Punkt 2 dotyka pliku, o którym mówi audyt (`CheckedHistoryIO.h`) — jeśli wolisz
nie ruszać tej warstwy, wystarczy punkt 1 (sprzątanie przy starcie).

---

## 7. Czego symulacja nie sprawdza (uczciwie)

- **kompilacji** `main_centrala.cpp` — sandbox nie ma toolchainu ESP32; to robi `pio run`,
- **prawdziwego LittleFS** przy zaniku prądu — model zakłada najgorszy przypadek
  (brak miejsca = zero bajtów albo pełny blok, bez „połówek”); F4 jest więc
  scenariuszem „jeżeli ogon kiedykolwiek będzie urwany”, a nie dowodem, że littlefs go urwie,
- **pojemności Twojej partycji** — dlatego wszystkie liczby są w bajtach,
- Firebase, Telegram, WDT, pompy, serwo — te ścieżki nie były ruszane.

---

## 8. Jak powtórzyć

```
bash narzedzia/limity-3.57.9/run.sh
FIRMWARE_SRC=/sciezka/src/main_centrala.cpp bash narzedzia/limity-3.57.9/run.sh
```

Pełne wyjście zapisane w `WYNIKI-symulacja.txt` i `WYNIKI-zgodnosc.txt`.

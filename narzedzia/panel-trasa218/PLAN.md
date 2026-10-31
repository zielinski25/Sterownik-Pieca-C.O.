# Etap 218 — plan i tabela ryzyk

## Po co ten etap

Po etapach 212–217 została jedna nieprzetestowana warstwa: **tabela tras
serwera**. Każdy poprzedni test wołał handler bezpośrednio, więc nie mógł
zobaczyć, że serwer wybierze inny handler niż zamierzony. Tutaj warstwa
`panel → tabela tras → handler` jest sprawdzana w całości.

## Kolejność pracy (wykonana)

| # | Krok | Stan |
|---|---|---|
| 1 | `trasy.py` — statyczna analiza tabeli tras w pliku po patchu | zrobione, 46 tras |
| 2 | Znalezienie dwóch martwych tras w v3.56.3 (`/history/long`, `/history/link`) | zrobione |
| 3 | `zrob_patch.py` — patch v3.56.3 → v3.56.4 (`exact()` na trzech trasach) | +27 −6, 3 hunki |
| 4 | `build.py` — budowa v3.56.4 od zera + wycięcie rejestracji i panelu | 10 PASS |
| 5 | `panel218.mjs` — prawdziwy skrypt panelu w V8 (model DOM) | 37 sprawdzeń |
| 6 | `Test.cpp` — adresy z panelu podane prawdziwej tabeli tras | 78 sprawdzeń |
| 7 | Mutanty M1–M5 | 5/5 czerwone |
| 8 | Paczka ZIP + pokwitowanie | patrz `../paczki-udostepnione/` |

## Tabela ryzyk

| Ryzyko | Czy tu pokryte | Jak |
|---|---|---|
| trasa nieosiągalna, bo wcześniejsza łapie jej adres | **tak** | `trasy.py` + Test.cpp grupa 02 + mutanty M1–M3 |
| panel woła adres, którego nie ma w tabeli | **tak** | adresy brane z `panel218.mjs`, nie przepisywane; grupa 03 |
| panel otwiera złą trasę dla długiej historii | **tak** | mutant M4 + grupa 05 (`w.trasa == 1`) |
| bilet dlugi przyjęty przez trasę krótką | **tak** | grupa 06 (401 w obie strony) |
| brak `Content-Disposition` (plik bez nazwy) | **tak** | grupa 04/05 — nazwy `historia-krotka.csv` / `historia-dluga.csv` |
| bilet wielokrotnego użytku | **tak** | grupa 04 — drugie użycie 401 |
| bilet bez terminu | **tak** | grupa 09 — po 6 minutach 401 |
| stara droga z `Authorization: Bearer` zepsuta poprawką | **tak** | grupa 10 |
| panel bez kontroli długości biletu | **tak** | mutant M5 |
| `?k=` z dodatkowym `&s=` (nieużywany parametr) | **tak** | `build.py` odrzuca `&s=` w panelu; `panel218.mjs` sprawdza brak |
| zła kolejność `#include` po patchu (błąd D1 z etapu 214) | **tak** | patch nie rusza include'ów; `build.py` porównuje SHA |
| kompilacja całego pliku na ESP | **nie** | użytkownik, PlatformIO |
| wygląd przycisku w telefonie | **nie** | poza zakresem (CSS nie jest testowany) |

## Czego świadomie NIE zmieniam

- panelu WWW (działa; poprawka jest tylko w tabeli tras),
- `HistoriaPobranie.h` (zostaje v3.56.3 z etapu 216),
- pozostałych 43 tras — `trasy.py` potwierdza, że żadna inna nie jest martwa,
- rytmu 8 s / 60 s, OTA, NVS, Telegrama.

# Firmware centrali 3.58.11 — alarm dymu z symulacji gaśnie razem z symulacją

Data: 2026-11-24 · następna po **3.58.10** (`firmware-patch/v3.58.10-zew2-ponowienie/`)
i zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (50 873 linie, SHA-256 `471b61b4…`).
Pliki `.h`, `secrets.h`, biblioteki i `platformio.ini` bez zmian. Patch względem 3.58.10:
**9 hunków, +91/−10** — blok `DYM-SYM` przed `pozar()`, 4 linie w `pozar()`, 1 pole w `/api/status`.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja

Zgłoszenie 22.11 18:25 (panel WWW → „Czujnik dymu i spalin” → Symulacja): wartość 2202 ADC
daje alarm — i słusznie — ale po **„Wyłącz teraz”** alarm dalej świeci. Panel pokazuje
„Symulacja wyłączona; alarm sterownika nadal aktywny — sprawdź kotłownię”, kafelek
CZUJNIK DYMU „1 ADC · próg alarmu przekroczony”.

Przyczyna — zachowanie **zamierzone**, ale przy testach mylące:

* alarm POŻAR jest zatrzaskowy; gaśnie **tylko na świeżym, prawdziwym odczycie**
  `< progAlarmowyDymu − HISTEREZA_DYMU` (500 − 30 = 470), nigdy „na słowo”;
* czujnik MQ w trybie impulsowym (WŁ 130 s, WYŁ 60 s, stabilizacja 90 s) daje świeży odczyt
  tylko w ostatnich ~40 s okna WŁ. Po wyłączeniu symulacji: ≤ 10 s przedłużenia + WYŁ 60 s +
  rozgrzewanie 90 s → **pierwszy prawdziwy pomiar po 2,5–3 min** i dopiero on gasił alarm.

Wartość „1 ADC” to ostatni prawdziwy odczyt — poniżej 470, więc alarm i tak by zgasł, tylko później.

## Co zmienia 3.58.11

1. **Źródło alarmu.** Przy aktywacji alarmu POŻAR `pozar()` zapamiętuje, czy odczyt, który go
   wywołał, był symulowany (`alarmDymuZSymulacji = symDymTeraz`). Prawdziwy świeży odczyt nad
   progiem w trakcie takiego alarmu robi z niego zwykły alarm (flaga gaśnie).
2. **Koniec symulacji kasuje alarm z symulacji.** `dymAlarmPoKoncuSymulacji()` wołana raz na tick
   `pozar()` (co ~5 s) zaraz po `symulacjaTrwa()`: gdy symulacja właśnie się skończyła
   („Wyłącz teraz” w WWW, `symuluj_stop` z Firebase/Telegrama, upływ czasu) i bieżący alarm
   pochodzi **wyłącznie** z symulacji — alarm jest kasowany tym samym sprzątaniem co zwykła
   dezaktywacja (`dymAlarmSkasuj()`: buzzer, wyciszenie, `dymNadzorPoSkasowaniuAktywny`).
   Log: `tag=ALARM lvl=INFO zdarzenie=dezaktywacja typ=POZAR powod=koniec_symulacji`.
   Jeśli w tym samym ticku czujnik daje prawdziwy odczyt nad progiem — alarm natychmiast
   wraca jako prawdziwy (log: dezaktywacja, potem aktywacja).
3. **Alarm z prawdziwego pomiaru — bez zmian.** Włączenie i wyłączenie symulacji go nie rusza;
   gaśnie tylko prawdziwym pomiarem `< 470`. Dotychczasowa linia logu dezaktywacji bez zmian.
4. `/api/status`: nowe pole `"dym_alarm_sym"` (true = bieżący alarm powstał na odczycie
   symulowanym). Panel LCD i aplikacja go nie wymagają (część C testów: zgodne).

Sprzątanie po alarmie, dotąd wpisane wprost w `pozar()`, jest teraz w jednej funkcji
`dymAlarmSkasuj(powod)` — oba miejsca (zwykła dezaktywacja i koniec symulacji) robią dokładnie to samo.
Koszt: 2 B RAM, ~200 B kodu, zero dodatkowych odczytów. Numery linii: `WYNIKI/zakres-zmian.txt`.

W panelu WWW napis „Symulacja wyłączona; alarm sterownika nadal aktywny” może mignąć na jedno
odświeżenie (do 5 s) — potem status przychodzi już bez alarmu. `Piec.html` bez zmian.

## Testy (PC, bez płytki) — `tests/`, wyniki w `WYNIKI/`

| część | co | wynik |
|---|---|---|
| A | blok `DYM-SYM` + **prawdziwy fragment `pozar()`** (aktywacja/dezaktywacja) wycięty z pliku, 6 scenariuszy | 21/21 |
| B | znaczniki 3.58.11, brak starych fragmentów, patch na 3.58.10 + `zrob_35811.py` → ta sama suma SHA | OK |
| C | pola `/api/status` a pola czytane przez panel LCD | zgodne (82 pola, panel czyta 47) |
| D | testy paczek 3.58.10 (ZEW2-FILTR 66, `wilgotnosc()` 47) i 3.58.7/3.58.6 (40 + 166 + 2697) na tym pliku | bez zmian, 0 błędów |
| nagłówki ESP32 | `DYM-SYM` + fragment `pozar()` oraz bloki 3.58.10/3.58.7/3.58.6 prawdziwymi nagłówkami arduino-esp32 3.3.8 (clang) | bez błędów, 0 ostrzeżeń |

`./tests/uruchom_testy.sh`, `RDZEN=3.3.8 ./tests/sprawdz_naglowki_esp32.sh`.

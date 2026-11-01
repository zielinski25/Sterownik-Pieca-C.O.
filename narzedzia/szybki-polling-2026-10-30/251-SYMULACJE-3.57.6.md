# 251 — Symulacje przed wgraniem: firmware 3.57.6

Data: 2026-10-31 · baza: **3.57.3** · zastępuje **3.57.5** i **3.57.4**
Pytanie użytkownika: „jeszcze przed wgraniem chcę, abyś zrobił obszerne symulacje,
aby potwierdzić poprawność”.

Odpowiedź w jednym zdaniu: **symulacje znalazły dwie dziury na zanik prądu, obie
naprawione; 3.57.5 nie nadaje się do wgrania, 3.57.6 jest gotowa.**

- ZIP firmware 3.57.6: `14p8XKDBj9pwp7bOghICpOkGZ_Fa5wwhz` · 751 440 B ·
  SHA-256 `f865e50bb07b235977860622d6b0bb9bf0cf0cc9c486a853651efc8e12e30355`
  https://drive.google.com/file/d/14p8XKDBj9pwp7bOghICpOkGZ_Fa5wwhz/view
- Aplikacja **0.31.64 bez zmian** (protokół ten sam, pole `hist_fs` i komendy `histfs_*`)

---

## 1. Co było symulowane

| warstwa | jak |
|---|---|
| protokół czyszczenia przy starcie | **prawdziwe klasy z repo** (`HistoryBoot.h`, `HistoryStartup.h`, `Storage.h`, `Transition.h`) skompilowane na hostowym „dysku” w pamięci — 17 scenariuszy |
| funkcje firmware | `historiaFsCzyscZadaj()`, `historiaFsNaprawUszkodzoneZadanie()` przepisane krok po kroku; zgodność przepisu ze źródłem pilnuje test statyczny |
| przełącznik zapisu | model 1:1 z `historiaZapiszProbke()`, `historiaFlushDoFS()`, `historiaDlugaZapiszProbke()` — 1000 próbek, wyłączanie, 50 przełączeń |
| panele | **prawdziwy kod** wycięty z `Piec.html` i z panelu w firmware (`flagKeys.forEach`, `toggleRow`, `live$`) uruchomiony na prawdziwym DOM (jsdom) |
| aplikacja Android | model `intField("hist_fs", 0, 1)` z `PiecState.kt` |

```
test-hist-fs.js       33 OK / 0 FAIL
symulacja C++         80 OK / 0 FAIL   (kompilacja: g++ -std=gnu++17 -Wall -Wextra, 0 ostrzeżeń)
panel w jsdom         26 OK / 0 FAIL
```

Uruchomienie: `bash narzedzia/symulacje-firmware/run-all.sh`

## 2. Co symulacja wykryła (i co z tego wynikło)

### Dziura 1 — zapis zadania nie był atomowy

`open("/history_reset_a1.req", "w")` **najpierw skraca plik do zera**, a potem
wpuszcza 17 bajtów. Zanik prądu w tym oknie zostawia plik pusty albo krótszy.
`Transition` każde odstępstwo od `RESET-HISTORY-A1\n` traktuje jako błąd →
`history194::Startup` nie wstaje → **historia nie działa do końca życia urządzenia**
(edytor plików w panelu blokuje tę ścieżkę, więc nie da się jej naprawić zdalnie).

S8 w symulacji to odtworzył: `.req` 9 B → `A1 = Blocked`, `Reason = RequestRead`,
`Startup` nie wstaje, CSV zostają. Stan trwały.

**Naprawa:** zapis do `.req.tmp` → odczyt kontrolny (17 B, `memcmp`) → `rename()`
na `.req`. Rename w LittleFS jest atomowy i nadpisuje cel
(`littlefs-project/littlefs#777`), więc zadanie jest albo w całości, albo go nie ma.
Wiara w plik tymczasowy nie jest ślepa — S6/S7 to sprawdzają (zanik w trakcie
zapisu `.tmp` i między zapisem a rename).

### Dziura 2 — brak naprawy uszkodzonego zadania przy starcie

Nawet zadanie zapisane przez starszą wersję albo przez obcy zapis blokowało
historię na zawsze. **Naprawa:** `setup()` (po mount, **przed** `historiaBoot.start()`)
czyta `.req`, a jeśli treść lub długość się nie zgadza — przenosi go do
`.req.bad`. Pliki CSV zostają nietknięte: uszkodzone zadanie to nie zadanie
czyszczenia (S8 → po naprawie historia startuje, dane całe).

### Dwie mniejsze

3. **Po ponownym włączeniu zapisu** resztka z kolejki czekała do 5 próbek
   (≈ 5 min). Teraz `histfs_wl` zgłasza natychmiastowy flush (S18c).
4. **Przełącznik w panelu nie wracał** po odrzuconej komendzie — checkbox
   kłamał do zamknięcia arkusza. Teraz `.then(ok => ...)` cofa go do stanu
   z centrali (B6), a `live$` przepisuje pole tylko przy **zmianie** statusu,
   żeby nie cofać kliknięcia użytkownika (B3).

## 3. Co potwierdzone (wybór)

| scenariusz | wynik |
|---|---|
| brak znaczników → start | `A1 = Existing`, CSV nietknięte, rozmiary z dysku |
| komenda → restart | oba CSV usunięte, `.done` = `HISTORY-A1-COMPLETE\n`, `.req` zjedzone, rozmiary 0 |
| zadanie bez usunięcia `.done` | **pliki zostają** — dlatego `.done` znika pierwsze |
| 5 cykli czyszczenia pod rząd | każdy kasuje, brak śmieci |
| zanik po `.done`, przed `.req` | historia działa, pliki nietknięte |
| zanik w trakcie zapisu `.tmp` | `.req` nie powstaje, historia startuje |
| błąd I/O przy usuwaniu `.done` | komenda zwraca `false`, zadanie nie złożone |
| wylosowane: 1000 próbek przy WYŁ | plik nie urósł **ani o bajt**, RAM ma wszystkie 1000 |
| 50 przełączeń co 3 próbki | czas rosnący, zero duplikatów, na flash tylko próbki z włączonym zapisem |
| panel: klik WYŁ, brak ACK | checkbox wraca do prawdy z centrali |

## 4. Czego symulacja NIE sprawdza

- **kompilacji `main_centrala.cpp`** — sandbox nie ma toolchainu ESP32 (to robi `pio run`),
- prawdziwego zachowania `littlefs` przy zaniku prądu — model zakłada **najgorszy**
  przypadek (plik częściowo zapisany), więc wynik jest ostrożniejszy niż rzeczywistość,
- Firebase, TLS, WDT, pompy, serwa — te ścieżki nie były ruszane.

## 5. Ograniczenie wilgotności (uczciwie)

Poprawka „0,0 % = błąd” działa, gdy zero **przerywa** normalny pomiar. Jeśli
czujnik jest **martwy od rozruchu**, „poprzednia dobra wartość” to nadal 0,0
i takie zera będą w plikach do wymiany czujnika. Pokazywanie „—” zamiast 0,0
wymaga nowego pola w statusie i nowej wersji aplikacji — do decyzji, nie blokuje
wgrania.

## 6. Wniosek

**Do wgrania: 3.57.6.** Nie wgrywać 3.57.4 ani 3.57.5 (zastąpione), ani
3.55.3–3.55.8 (wycofane wcześniej).

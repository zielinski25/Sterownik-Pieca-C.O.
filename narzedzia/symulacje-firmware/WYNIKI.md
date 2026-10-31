# Symulacje 3.57.6 — wyniki

Data: 2026-10-31 · firmware: **3.57.6** · uruchomienie: `bash narzedzia/symulacje-firmware/run-all.sh`

```
════════ 1/3  Test statyczny (firmware + oba panele) ════════   33 OK / 0 FAIL
════════ 2/3  Symulacja A1 i bramek zapisu (C++)        ════════   80 OK / 0 FAIL
════════ 3/3  Symulacja panelu w DOM (jsdom)            ════════   26 OK / 0 FAIL
                                                          RAZEM  139 OK / 0 FAIL
```

Czas: ~2 s. Kompilator C++: `g++ -std=gnu++17 -Wall -Wextra` — **zero ostrzeżeń**.

---

## Co jest symulowane, a co nie

| element | jak |
|---|---|
| protokół czyszczenia przy starcie | **prawdziwe klasy z repo** (`HistoryBoot.h`, `HistoryStartup.h`, `Storage.h`, `Transition.h`) na pamięciowym systemie plików — kompilowane, nie przepisane z pamięci |
| funkcje firmware `historiaFsCzyscZadaj()`, `historiaFsNaprawUszkodzoneZadanie()` | przepisane krok po kroku; **zgodność przepisu ze źródłem** pilnuje test statyczny (te same nazwy plików, kolejność, 17 B, odczyt kontrolny, rename) |
| bramki zapisu (przełącznik) | model 1:1 z `historiaZapiszProbke()` / `historiaFlushDoFS()`; ciągi znaków warunków sprawdzane w źródle |
| panele (Piec.html i panel w firmware) | **prawdziwy kod** wycięty z plików (`flagKeys.forEach`, `toggleRow`, `live$`) uruchomiony na prawdziwym DOM (jsdom) |
| aplikacja Android | model `intField("hist_fs", 0, 1)` z `PiecState.kt` |
| **kompilacja `main_centrala.cpp`** | **NIE** — brak toolchainu ESP32 w sandboxie |

---

## Scenariusze (C++)

### Protokół czyszczenia

| # | scenariusz | wynik |
|---|---|---|
| S1 | zwykły start, brak znaczników | nic nie ruszane, `Existing`, rozmiary z dysku |
| S2 | komenda → restart | oba CSV usunięte, `.done` zapisane, `.req` zjedzone, rozmiary 0 |
| S3 | zadanie bez usunięcia `.done` (stary błąd) | **pliki NIE usunięte** ← dlatego `.done` znika pierwsze |
| S4 | 5 cykli pod rząd | każdy kasuje, `.done` odtwarzane, brak śmieci |
| S5 | zanik prądu po usunięciu `.done` | historia startuje, pliki nietknięte |
| S6 | zanik prądu **w trakcie** zapisu `.tmp` (9 z 17 B) | `.req` nie powstaje, historia startuje — **poprawka 3.57.6** |
| S7 | zanik prądu po zapisie `.tmp`, przed `rename` | `.tmp` pełne, `.req` brak, historia startuje |
| S8 | **wariant 3.57.5**: zapis bezpośredni, zanik w połowie | `Transition` **blokuje** (`RequestRead`), historia **nie wstaje** → naprawa przenosi `.req` do `.req.bad` → historia startuje, CSV nietknięte |
| S9 | naprawa przy poprawnym `.req` | naprawa nic nie robi, zadanie wykonane, pliki usunięte |
| S10 | błąd I/O przy usuwaniu `.done` | komenda zwraca `false`, zadanie nie złożone, historia żyje |
| S11 | nie da się założyć `.tmp` | `false`, brak `.req`/`.tmp`, pliki nietknięte |
| S12 | `rename` zawodzi | `false`, `.tmp` posprzątany, historia startuje |
| S13 | `.req` poprawny, ale `.done` istnieje | pliki **nie** usunięte, `.req` zjedzone (ścieżka „już czyszczono") |
| S14 | istnieje tylko `hist_short.csv` | usunięty, `.done` zapisane |
| S15 | LittleFS nie zamontowany | historia nie startuje, nic nie skasowane |
| S16 | CSV ucięty (bez `\n` na końcu) | `Startup` odrzuca (Boundary) — zachowanie istniejące, fail-closed |
| S17 | istnieje `hist_short.csv.rebuild` | start zatrzymany, brak zmian |

### Bramki zapisu (przełącznik)

| # | scenariusz | wynik |
|---|---|---|
| S18a | 1000 próbek, zapis WŁ | 1000 linii w pliku, kolejka pusta, RAM 1000 |
| S18b | 1000 próbek, zapis WYŁ | plik nie urósł **ani o bajt**, kolejka pusta, RAM 1000 |
| S18c | 7 próbek → wyłączenie → 100 próbek → włączenie | przez cały czas 5 linii; po włączeniu resztka dojeżdża **od razu** (7) |
| S18d | 50 przełączeń co 3 próbki | czasu rosnące, **bez duplikatów**, na flash tylko próbki z włączonym zapisem (75 z 150), RAM 150 |

---

## Scenariusze (panel — prawdziwy kod w DOM)

| # | scenariusz | wynik |
|---|---|---|
| A1–A12 | 6 statusów × 2 panele: `1`, `0`, brak pola, `"1"`, `null`, `true` | zgodnie z oczekiwaniem (`"1"` i `null` = „brak danych") |
| B1 | start: status WŁ | checkbox zaznaczony |
| B2 | klik WYŁ | wysłane `histfs_wyl` |
| B3 | status jeszcze stary przez 2 ticki | checkbox **nie wraca sam** (nie cofa użytkownika) |
| B4 | przyszedł status WYŁ | zgodny |
| B5 | zmiana zdalna na WŁ | checkbox sam się zaznacza |
| B6 | komenda odrzucona (NACK) | checkbox **wraca do stanu centrali** |
| B7 | brak pola `hist_fs` | brak wyjątku, przełącznik odznaczony |

---

## Co symulacje wykryły (i co poprawiono w 3.57.6)

1. **Zapis zadania nie był atomowy.** `open("w")` najpierw skraca istniejący `.req` do zera, potem wpuszcza 17 bajtów. Zanik prądu w tym oknie zostawiał puste/uszkodzone zadanie, a `Transition` traktuje to jako błąd → **historia nie wstaje nigdy** (edytor plików w panelu te ścieżki blokuje). Teraz: `.req.tmp` + odczyt kontrolny + `rename()`.
2. **Brak naprawy przy starcie.** Nawet cudzy/uszkodzony `.req` blokował historię na stałe. Teraz `setup()` przenosi taki plik do `.req.bad` przed startem historii — CSV zostają nietknięte, bo uszkodzone zadanie to nie zadanie czyszczenia (S8).
3. **Po ponownym włączeniu resztka czekała do 5 próbek** (≈5 min). Teraz `histfs_wl` zgłasza natychmiastowy flush (S18c).
4. **Przełącznik w panelu nie wracał po nieudanej komendzie** — checkbox kłamał do zamknięcia arkusza. Teraz `.then(ok => ...)` cofa go do stanu z centrali (B6).

Punkty 1–2 to skutek uboczny mojego własnego kodu z 3.57.5 — symulacja je znalazła, zanim to poszło na piec.

---

## Czego to nie zastępuje

- Nie sprawdza kompilacji `main_centrala.cpp` (brak ESP32 toolchainu) — to robi `pio run`.
- Nie sprawdza zachowania prawdziwego `littlefs` przy zaniku prądu; model zakłada **najgorszy** przypadek (plik częściowo zapisany), więc wynik jest ostrożniejszy niż rzeczywistość.
- Nie sprawdza Firebase, TLS, WDT, pompy, serwa — te ścieżki nie były ruszane.

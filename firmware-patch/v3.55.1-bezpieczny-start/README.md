# v3.55.1 — BEZPIECZNY START (24.10.2026)

## Co to naprawia

24.10.2026 centrala przestała odpowiadać: cisza w Telegramie, brak `PiecCO_AP`,
brak panelu pod adresem IP, brak danych w aplikacji. Log z USB pokazał:

```
tag=HISTORIA-FS zdarzenie=wczytano_z_dysku probek=1440
E (34638) task_wdt: Task watchdog got triggered ... CPU 1: loopTask
E (34638) task_wdt: Aborting.
Rebooting...
```

**Przyczyna:** `/hist_long.csv` urósł do 1 465 977 B, a funkcja
`historiaDlugaWczytajZFS()` czytała go **jednym** `f.readString()`
(bajt po bajcie, doklejanie do `String`), po czym skalowała na wiersze
`String::substring()`. Wczytywanie przekroczyło okno Task WDT (15 s),
watchdog ubił `loopTask` **przed** startem WiFi → pętla restartów →
urządzenie zniknęło z sieci.

**Wersja firmware nie miała tu nic do rzeczy** — 3.54.0 ma ten sam kod,
dlatego powrót do niej nic nie zmieniał.

## Co zmieniono

| Warstwa | Co robi | Po co |
|---|---|---|
| Porcje 4 KB | plik czytany `f.read(buf, 4096)`, po każdej porcji `serwisujTWDTJesliZarejestrowany()` + `vTaskDelay(1)` | żadne zatrzymanie nie trwa dłużej niż kilka ms, IDLE1 karmi WDT |
| Ogon pliku | czytane tylko ostatnie 512 KB (krótki) / 2 MB (długi) | start nie wydłuża się wraz z rozmiarem pliku |
| Limit czasu | 4 s (krótki) / 9 s (długi), potem STOP i dalszy start | start NIGDY się nie zawiesi |
| Pierścień | przy pliku większym niż bufor nadpisywane są NAJSTARSZE próbki | wcześniej wszystko powyżej limitu było po cichu odrzucane |
| Pusta partycja | format tylko gdy obszar to same `0xFF` | po ręcznym wyczyszczeniu partycji urządzenie samo tworzy system plików |

Czego **nie** zmieniono: format CSV, kolejność i znaczenie kolumn,
`/api/history*`, zapis na flash, resync, telemetria, Firebase, Telegram, OTA.

## Testy (hostowe, bez ESP32)

```
tests/host/run_history_tests.py   48 sprawdzen, 0 bledow
tests/host/run_tests.py           20 sprawdzen, 0 bledow  (telemetria v3.55.0, bez zmian)
```

Test historii sprawdza m.in.: zgodność ze starym parserem (także gdy `\n`
wypada na granicy porcji), brak dużej alokacji, czytanie od ogona,
przerwanie po limicie czasu, format 35 i 36 kolumn, CRLF, brak pliku.

## Co jest w paczce

```
main_centrala.cpp                      <- pełny plik, gotowy do podmiany
main_centrala-v3.55.0-to-v3.55.1.patch <- alternatywa: patch -p1
secrets.h.example                      <- wzór (twoje hasła zostają lokalnie)
tests/host/…                           <- testy
SHA256SUMS.txt
```

## Jak wgrać (użytkownik robi to sam)

1. Podmień `src\main_centrala.cpp` w swoim projekcie na plik z tej paczki.
2. `secrets.h` w `src\` musi zostać — bez niego kompilacja się nie zacznie
   (to celowe).
3. Zbuduj i wgraj:

```powershell
pio run -t upload
```

albo w PlatformIO: **Upload**.

4. Po restarcie sprawdź `PiecCO_AP` i Telegram.

## Czego ta wersja NIE robi

Nie naprawia zadania `tgTask` (nadzór + auto-restart po zawieszeniu) —
to osobna zmiana, do zrobienia po odzyskaniu dostępu.

## Historia wersji

- v3.55.0 — jedna paczka telemetrii naraz (24.10.2026, Release na GitHubie)
- **v3.55.1 — bezpieczny start: historia czytana strumieniowo (24.10.2026)**

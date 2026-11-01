# Symulacja limitów — firmware 3.57.8 (krok 1 przed 3.57.9)

Data: 2026-11-01 · baza: **3.57.8** (`SterownikCO-v3.57.8-HIST-FS-OPCJA.zip`,
SHA-256 paczki sprawdzony, `src/main_centrala.cpp` = `db3da885…`)

To jest **pierwszy krok z ustalonej kolejności**: najpierw symulacje limitów,
dopiero potem zmiany w firmware. Ten katalog nie zmienia firmware — mierzy,
gdzie historia na flashu się kończy.

## Po co

Do 3.57.8 historia na flashu była testowana pod kątem *poprawności*
(atomowe zadanie A1, przełącznik `hist_fs`, brak wieszania) — nie pod kątem
*pojemności*. A pojemność ma twarde granice, których nikt nie policzył:

| limit | wartość | skąd |
|---|---|---|
| próbek w buforze krótkim | 1440 (rotacja po 2441) | `HISTORIA_MAX_PROBEK` + `HISTORIA_RESYNC_MARGINES` 1000 |
| próbek w buforze długim | 8000 (rotacja po 8501) | `HISTORIA_DLUGA_MAX_PROBEK` + margines 500 |
| kolejka RAM linii CSV | 4096 B | `HISTORIA_PENDING_LIMIT_B` |
| rezerwa „na historię długą" | 2 MB | `FS_RESERVE_HIST_LONG_B` (tryb SHADOW — nic nie egzekwuje) |
| miejsce na dysku | **nieznane z repo** | brak `partitions.csv` w repozytorium |

## Jak jest zrobione

Nie przepisujemy logiki „z pamięci”. Uruchamiamy **prawdziwą klasę
`history193::Engine`** z paczki 3.57.8:

- `CheckedHistoryIO.h` — kopia **bit w bit** (`SHA-256 8eb18693…`, zgodna
  z `SHA256SUMS.txt` paczki), kompiluje się na hoście bez zmian,
- `flash_sym.h` — podstawiony pod `history193::Calls` dysk o **skończonej
  pojemności**: zajęcie liczone po blokach 4 KB jak w LittleFS, zapis ponad
  pojemność kończy się `ENOSPC`, `rename()` podmienia cel, `O_EXCL`
  na istniejącym pliku kończy się `EEXIST`,
- `test_limity.cpp` — warstwa wyżej, 1:1 z firmware: append → (gdy
  `linie > limit + margines`) trim; błąd blokuje `Engine` dokładnie tak,
  jak w `historiaFlushDoFS()`.

Rozmiary linii są **zmierzone z prawdziwych plików centrali** (`hist_short (9).csv`,
`hist_long (7).csv` z Dysku): **71 B** i **180 B** na linię.

`test_zgodnosc.py` pilnuje, żeby symulacja nie odjechała od źródła: czyta
`main_centrala.cpp` i sprawdza progi, warunek rotacji, jeden wspólny `Engine`,
brak kasowania `.rebuild`, rezerwę 2 MB, komunikaty i limit 15 znaków w kluczach NVS.

## Uruchomienie

```
bash narzedzia/limity-3.57.9/run.sh
FIRMWARE_SRC=/sciezka/src/main_centrala.cpp bash narzedzia/limity-3.57.9/run.sh
```

Bez `FIRMWARE_SRC` część „zgodność” kończy się `POMINIETO`. Źródło 3.57.8 leży teraz
**w tej samej gałęzi**: `firmware-patch/v3.57.8-raport-i-wersja/src/main_centrala.cpp`,
więc wystarczy:

```
FIRMWARE_SRC=firmware-patch/v3.57.8-raport-i-wersja/src/main_centrala.cpp \
  bash narzedzia/limity-3.57.9/run.sh
```

## Co dalej z tymi znaleziskami

**Decyzja użytkownika (2026-11-01):** tematy historii/`.rebuild` (F2, F4 z raportu)
idą **osobno w 3.57.10** — 3.57.9 zajmuje się wyłącznie limitami sieci.
Raport z tego kroku zostaje jako gotowa lista do 3.57.10.

## Wynik

**38 / 38** scenariuszy C++ i **19 / 19** kontroli zgodności — wszystko zielone.
Pełne wyjście: `WYNIKI-symulacja.txt`, `WYNIKI-zgodnosc.txt`.
Wnioski i to, co z nich wynika dla 3.57.9: **`RAPORT-LIMITY-3.57.9.md`**.

## Pliki

| plik | rola |
|---|---|
| `CheckedHistoryIO.h` | prawdziwy `history193::Engine`, kopia bit w bit z 3.57.8 |
| `flash_sym.h` | dysk o skończonej pojemności (model `history193::Calls`) |
| `test_limity.cpp` | scenariusze L1–L8 |
| `test_zgodnosc.py` | kontrola, że symulacja opisuje prawdziwe źródło |
| `run.sh` | kompilacja + oba zestawy |
| `WYNIKI-symulacja.txt`, `WYNIKI-zgodnosc.txt` | zapisane wyjście |

## Czego to NIE sprawdza

- kompilacji `main_centrala.cpp` (sandbox nie ma toolchainu ESP32 — to robi `pio run`),
- prawdziwego zachowania LittleFS przy zaniku prądu (model zakłada najgorszy
  przypadek: brak miejsca = zero bajtów albo pełny blok, bez „połówek”),
- `LittleFS.totalBytes()` na Twojej centrali — dlatego raport podaje wymagania
  w bajtach, a nie w procentach.

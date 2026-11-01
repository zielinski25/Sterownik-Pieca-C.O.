# Firmware v3.56.7 — PRZYWRÓCENIE ZACHOWANIA v3.56.5

Data: 2026-10-29
Plik: `main_centrala.cpp` (2 378 664 B, 47 023 linie)
Baza: v3.56.6 (który sam był v3.56.5 + 3 zmiany)

---

## 1. Co to jest, jednym zdaniem

**To jest v3.56.5 z innym numerem wersji.** Cofnąłem wszystkie trzy zmiany, które
wprowadziłem w v3.56.6, bo po wgraniu piec zaczął się restartować.

## 2. Dowód, że to naprawdę v3.56.5

Porównałem plik v3.56.7 z oryginalnym v3.56.5, który mam w archiwum
(`N/naprawa223/paczka/PROJEKT-v3.56.5-POPRAWIONY.zip`). Różnice to **wyłącznie**:

| Co | Ile linii |
|---|---|
| Nowy komentarz na początku pliku (wyjaśnienie) | +18 |
| `FIRMWARE_VERSION` z `"v3.56.5"` na `"v3.56.7"` | 1 |
| Trzy komentarze `// [v3.56.7] PRZYWROCONE ...` | +3 |

**Razem 23 linie różnicy, wszystkie to komentarze albo numer wersji.**
Kod wykonywalny jest identyczny, linia w linię. Sprawdziłem też, że liczba
nawiasów `{` i `}` jest taka sama jak w v3.56.5 (7103 / 7098 — ta różnica 5
to stan odziedziczony z v3.56.5, nie moja zmiana; nie ruszałem go).

## 3. Co dokładnie cofnąłem

| # | zmiana w v3.56.6 | stan w v3.56.7 |
|---|---|---|
| 1 | `fbInitialize()` bezwarunkowo | **cofnięte** → `if (g_spoolRuntimeArmed \|\| g_telemetryPendingValid)` |
| 2 | próg ringu 15 → 2 próbki | **cofnięte** → 15 |
| 3 | próg wieku 600 → 180 s | **cofnięte** → 600 |
| 4 | `g_spoolRuntimeForceSpill` (wymuszony zapis flasha) | **usunięte w całości** |

Sprawdziłem: w pliku nie ma ani jednego odwołania do `g_spoolRuntimeForceSpill`.

## 4. Dlaczego wracam — liczby z logu

Log `piec_log (28).txt` (16 654 linii, 18.10 → 29.10 12:08):

**v3.56.5 (boot=2), 24.10 19:06 → 29.10 10:28 = 4 dni 15 godzin, ZERO restartów:**
- circuit breaker Firebase otwierał się **30 razy**
- `FB-CMD` timeout **24 razy**
- `FB-SEND` timeout **26 razy**

Piec to wszystko przetrwał. Sam Firebase nie wywraca pieca.

**v3.56.6 po OTA:**
- boot=3: 10:30:33 → 11:27:52 (57 min) — **zero błędów Firebase**, restart `powod=SW`
- boot=4: 11:30:00 → 12:03:00 (33 min) — od 11:59 lawina timeoutów,
  `tgTask` milczał 300 s, nadzór zrobił restart (raport `NADZOR nr=1`)

**Dwa restarty w ciągu godziny po wgraniu, po 4 dniach spokoju.** To wystarczy.

## 5. Czego NIE było przyczyną (sprawdzone, żeby nie zgadywać)

Pamięć i stos były w normie przez cały boot=4:
- `int_free` 96 672 B → 80 192 B (spadek ~16 kB, bez wycieku w dół)
- `hwm_tg` (szczyt stosu zadania Telegrama) **2 256–2 708 B** — daleko od limitu
- `fb_tasks` 0–3, wracał do 0 (brak zawieszonych zadań Firebase)

Więc to **nie** wyciek pamięci i **nie** przepełnienie stosu.

## 6. Czego świadomie NIE naprawiam

**Luki 10 próbek na wykresie zostają.** To był powód v3.56.6. Wracam do tego
później, ale inaczej: **jedna zmiana, potem 24 h obserwacji, potem następna.**
Poprzednio wrzuciłem trzy naraz i nie dało się ustalić, która szkodzi.

## 7. Zawartość paczki

```
main_centrala.cpp                          <- plik do src\ (2 378 664 B)
main_centrala-v3.56.6-to-v3.56.7.patch     <- ten sam plik jako łatka (123 linie)
README.md                                  <- ten plik
CZYTAJ-NAJPIERW.txt                        <- instrukcja krok po kroku
SHA256SUMS.txt                             <- sumy kontrolne
*.h (14 plików)                            <- bez zmian wobec v3.56.6
secrets.h.example                          <- wzór, NIE nadpisuj swojego src\secrets.h
```

**Pliki `.h` są bez zmian** — możesz ich nie ruszać. Wystarczy podmienić
`main_centrala.cpp`.

## 8. Wersja podbita?

**TAK.** `v3.56.6` → `v3.56.7`. Podbicie jest konieczne, bo nadzór i mechanizm
OTA porównują numery wersji — bez podbicia piec nie przyjąłby pliku jako nowszy.

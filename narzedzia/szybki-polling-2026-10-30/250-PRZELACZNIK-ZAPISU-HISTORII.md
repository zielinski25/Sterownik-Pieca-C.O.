# 250 — Przełącznik „Zapis historii na flash" (firmware 3.57.5 + aplikacja 0.31.64 + Piec.html)

Data: 2026-10-31 · baza firmware: **3.57.3** · baza aplikacji: **0.31.63**
**3.57.5 zastępuje 3.57.4** — ta wersja nigdy nie poszła na piec.

- ZIP firmware 3.57.5: `1zA0saSSZlgdXZejgcienBgg8Hf6F4EnI` · 753 435 B ·
  SHA-256 `3824c2057c44009353f80848f326d2f46f881163a2bb18b198e186ce109eddfd`
- ZIP aplikacji 0.31.64: `1z2L-yYsocX1yvWBnS03xm6Gt2RLh6Wu9` · **bez zmian**

---

## 1. Po co

`/hist_long.csv` rośnie do **~1,8 MB** (8000 próbek), `/hist_short.csv` do **~130 kB**
(1440 próbek, zapis co 5 próbek ≈ 5 min). Kto nie potrzebuje archiwum na flashu,
ten płaci za nie pamięcią i cyklami zapisu. Od 3.57.4/3.57.5 można to wyłączyć —
**jednym przełącznikiem dla obu plików**, z aplikacji, z `Piec.html` i z panelu centrali.

**Warunek użytkownika: „firebase ma działać".** Spełniony — wyłączenie dotyka
wyłącznie `append` do dwóch plików CSV.

## 2. Co dokładnie odcina wyłączenie

| ścieżka | działa po wyłączeniu? |
|---|---|
| telemetria do Firebase `/piec/telemetry/1m/v1` | **TAK** — osobny kod, osobna kolejka |
| bufor krótki w RAM (wykresy 6 h / 24 h, `/api/history`) | **TAK** — do restartu |
| bufor długi w RAM (`/api/history/long`) | **TAK** — do restartu |
| dopisywanie `hist_short.csv` | **NIE** |
| dopisywanie `hist_long.csv` | **NIE** |
| opróżnianie kolejki zapisu (resztka z RAM) | **NIE** — poprawka 3.57.5 |
| status, komendy, alarmy, pompa, serwo, OTA | **TAK** — bez zmian |

## 3. Przegląd przed wgraniem — trzy błędy 3.57.4 i poprawki

Przegląd kodu + research (littlefs #1237, dokumentacja Preferences/NVS, zachowanie
DHT22) wykazał trzy rzeczy do naprawy. Wszystkie są w 3.57.5.

### 3.1 Czyszczenie plików — dwa mechanizmy zamiast jednego (naprawione)

W firmware **już istniał** audytowany kanał jednorazowego czyszczenia przy starcie:
`history_a190::Transition` + `history_a191::Storage` + `EspVfsCalls`.
Warunki: plik `/history_reset_a1.req` o treści dokładnie `RESET-HISTORY-A1\n`
(17 bajtów), brak `/history_reset_a1.done`. Wtedy transition usuwa **oba** CSV,
potwierdza `stat`-em nieobecność, tworzy `.done` i zjada zadanie — wszystko
**przed** pierwszym odczytem i przed startem producentów.

3.57.4 robiło to własnym `LittleFS.remove()` po starcie, z drugim znacznikiem
w NVS (`hist/czysc`) i ręcznym zerowaniem pięciu liczników.

3.57.5: `histfs_czysc` **tylko uzbraja kanał A1**.

| krok | co robi | dlaczego |
|---|---|---|
| 1 | usuwa `/history_reset_a1.done` | gdy `.done` istnieje, transition idzie ścieżką „już czyszczono" (`release`) i **nie kasuje plików** |
| 2 | zapisuje `/history_reset_a1.req` = `RESET-HISTORY-A1\n` | `Storage::request()` czyta do 64 B i wymaga **dokładnie** 17 B + zgodnej treści |
| 3 | sprawdza zapis (`flush`, `close`, `exists`) | cisza przy nieudanym zapisie byłaby gorsza niż błąd |

**Dlaczego nie kasować „w locie"** — `littlefs-project/littlefs#1237`: usunięcie
pliku, do którego istnieje otwarty uchwyt, potrafi nadpisać **obcy** wpis
w katalogu przy następnym `sync` (katalog zwraca `LFS_ERR_CORRUPT`). Dokładnie
taki układ jest u nas: `historiaTaskFn` dopisuje co 5 próbek.

### 3.2 Panel centrali i `Piec.html` nie czytały `hist_fs` (naprawione)

Oba panele parsują status po **białej liście** (`flagKeys`, `numericRules`,
`textKeys`). `hist_fs` nie było w żadnej z nich → `hasData('hist_fs')` zawsze
`false` → kafelek pokazywałby „Ta wersja firmware nie zgłasza tego ustawienia"
**nawet na 3.57.5**. Dodane do `flagKeys` i do stanu początkowego `S`;
przełącznik dogania status przez `live$` (przepisuje pole tylko przy realnej
zmianie wartości, żeby nie cofać kliknięcia co 2 s).

### 3.3 Wyłączenie nie zatrzymywało kolejki (naprawione)

Bramka w 3.57.4 obejmowała tylko **nowe** próbki. Resztka czekająca w
`historiaPendingCsv` (do 4 KB) mogła jeszcze dolecieć na flash.
Teraz bramka jest też w `historiaFlushDoFS()` i `historiaDlugaFlushDoFS()` —
po wyłączeniu **żadnego dopisania**, a resztka czeka w RAM i po ponownym
włączeniu jedzie jednym appendem.

## 4. Firmware 3.57.5 — mapa zmian w kodzie

| # | miejsce | zmiana |
|---|---|---|
| 1 | nagłówek pliku | blok changelogu 3.57.4 → 3.57.5 |
| 2 | `FIRMWARE_VERSION` | `v3.57.4` → `v3.57.5` |
| 3 | po `historiaClearPending` | `g_histFsZapisWlaczony` + `historiaZapisNaFsWlaczony()`, `historiaFsUstaw()`, forward-decl `historiaFsCzyscZadaj()` |
| 4 | bramka krótkiej historii (`historiaZapiszProbke`) | `if (!historiaClearPending && historiaZapisNaFsWlaczony())` |
| 5 | bramka długiej historii (`historiaDlugaZapiszProbke`) | `... && historiaZapisNaFsWlaczony() && historiaDlugaFlushKolejka` |
| 6 | `historiaFlushDoFS()` | `... \|\| !historiaZapisNaFsWlaczony()` — **nowe w 3.57.5** |
| 7 | `historiaDlugaFlushDoFS()` | to samo — **nowe w 3.57.5** |
| 8 | `/piec/status` | pole **`hist_fs`** (1/0) |
| 9 | `/api/status` | to samo pole dla panelu lokalnego |
| 10 | dispatcher komend | `histfs_wl`, `histfs_wyl`, `histfs_czysc` |
| 11 | przed `setup()` | `historiaFsCzyscZadaj()` (kanał A1) + `historiaFsInicjalizujPrzyStarcie()` (tylko NVS + `Serial.printf`) |
| 12 | `setup()` przed `historiaWczytajZFS()` | wywołanie punktu 11 |
| 13 | DHT22 (wilgotność) | `... \|\| wilgotnoscOdczytanaLok <= 0.1f` |
| 14 | panel WWW centrali | kafelek + `showHistFsSheet()` + `hist_fs` w `flagKeys`/`S` |

Razem **+246 linii / −9** względem 3.57.3 (18 hunków).

### NVS

namespace **`hist`**, klucz **`fs_on`** (bool; brak = włączone). Klucz `czysc`
zniknął — nie ma drugiego mechanizmu. Zapis tylko przy zmianie ustawienia.

## 5. Wilgotność — poprawka z tego samego wydania

`read_dht()` potrafi zwrócić `DHT_OK` z zerem (ramka samych zer ma poprawną sumę
kontrolną: 0+0+0+0 = 0). Warunek sprawdzał tylko `isnan()`, więc fałszywe **0,0 %**
leciało do telemetrii i plików: zaniżało średnią archiwum o ~5 punktów i dawało
skoki **+100** na wykresie różnic (dokument 249, sekcja 0e).

```cpp
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f)
```

Zero wchodzi w gałąź błędu — zostaje poprzednia dobra wartość (`poprzedniaWilgotnosc`).

## 6. Aplikacja 0.31.64 — bez zmian

Protokół ten sam (`hist_fs`, `histfs_*`), więc nie ma nowego ZIP-a. Podpowiedź
w aplikacji „potrzebna 3.57.4" znaczy „3.57.4 albo nowsza".

| plik | zmiana (z 0.31.64) |
|---|---|
| `core/PiecState.kt` | pole `histFs` + `intField("hist_fs", 0, 1)` |
| `core/AppModel.kt` | `histFsState()`, `setHistFs()`, `clearHistFiles()` |
| `ui/screens/sheets/HistFs.kt` | nowy ekran (`CheckRowEcho` + przycisk czyszczenia) |
| `ui/screens/Lists.kt` / `Overlays.kt` | kafelek, tytuł, dispatch |

## 7. Zgodność

| firmware | aplikacja | efekt |
|---|---|---|
| 3.57.3 i starszy | 0.31.64 | ekran pokaże „brak danych" — **poprawnie**, brak pola `hist_fs` |
| **3.57.5** | 0.31.64 | wszystko działa w trzech miejscach |
| 3.57.4 | 0.31.64 | **nie wgrywać** — kafelek w panelach pokaże „brak danych" |

## 8. Sprawdzone / niesprawdzone

**Sprawdzone automatycznie** — `narzedzia/testy-firmware/test-hist-fs.js`, **27/27 OK**:

- treść i długość zadania A1 zgodna z `Storage.h` (`RESET-HISTORY-A1\n`, 17 B),
- ścieżka `.done` zgodna z `EspVfsCalls.h`; kolejność „najpierw `.done`, potem `.req`",
- `hist_fs` w obu JSON-ach i w `flagKeys` **obu** paneli + w stanie `S`,
- brak `putBool("czysc")`/`getBool("czysc")`, brak `LittleFS.remove` na plikach historii,
- bilans `{}`, `()`, `[]` w `main_centrala.cpp` po odjęciu literałów i komentarzy,
- składnia JS panelu i `Piec.html` — `node --check` na wyciągniętych blokach `<script>`.

**Niesprawdzone:** kompilacja — sandbox nie ma toolchainu ESP32. Jeśli `pio run`
zgłosi błąd, wklej log i poprawię.

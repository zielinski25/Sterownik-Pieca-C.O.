# 250 — Przełącznik „Zapis historii na flash" (firmware 3.57.4 + aplikacja 0.31.64 + Piec.html)

Data: 2026-10-31 · baza firmware: **3.57.3** · baza aplikacji: **0.31.63**

---

## 1. Po co

`/hist_long.csv` rośnie do **~1,8 MB** (8000 próbek), `/hist_short.csv` do **~130 kB**
(1440 próbek, zapis co 5 próbek ≈ 5 min). Kto nie potrzebuje archiwum na flashu,
ten płaci za nie pamięcią i cyklami zapisu. Od 3.57.4 można to wyłączyć —
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
| status, komendy, alarmy, pompa, serwo, OTA | **TAK** — bez zmian |

## 3. Firmware 3.57.4 — zmiany w kodzie

| # | miejsce | zmiana |
|---|---|---|
| 1 | nagłówek pliku | blok changelogu 3.57.4 |
| 2 | `FIRMWARE_VERSION` | `v3.57.3` → `v3.57.4` |
| 3 | po `historiaClearPending` | `g_histFsZapisWlaczony` + `historiaZapisNaFsWlaczony()`, `historiaFsUstaw()`, `historiaFsCzyscZadaj()` |
| 4 | bramka krótkiej historii (`historiaZapiszProbke`) | `if (!historiaClearPending && historiaZapisNaFsWlaczony())` |
| 5 | bramka długiej historii (`historiaDlugaZapiszProbke`) | `... && historiaZapisNaFsWlaczony() && historiaDlugaFlushKolejka` |
| 6 | `/piec/status` | nowe pole **`hist_fs`** (1/0) |
| 7 | `/api/status` | to samo pole dla panelu lokalnego |
| 8 | dispatcher komend | `histfs_wl`, `histfs_wyl`, `histfs_czysc` |
| 9 | przed `setup()` | `historiaFsInicjalizujPrzyStarcie()` — odczyt NVS + czyszczenie |
| 10 | `setup()` przed `historiaWczytajZFS()` | wywołanie punktu 9 |
| 11 | DHT22 (wilgotność) | `... \|\| wilgotnoscOdczytanaLok <= 0.1f` |
| 12 | panel WWW centrali | kafelek „Zapis historii na flash" + ekran `showHistFsSheet()` |

Razem **+179 linii**, bilans nawiasów `{}` `()` `[]` względem 3.57.3 **zbilansowany**
(9/9, 71/71, 0/0).

### NVS

namespace **`hist`**, klucze: `fs_on` (bool, brak = włączone), `czysc` (bool, jednorazowy).
Zapis tylko przy zmianie ustawienia — nie przy każdej próbce.

### Czyszczenie — dlaczego przy restarcie

Do plików pisze **wyłącznie** `historiaTaskFn`. Usunięcie pliku spod otwartego
strumienia to błąd I/O, więc `histfs_czysc` ustawia znacznik w NVS, a
`historiaFsInicjalizujPrzyStarcie()` (w `setup()`, **przed** `historiaWczytajZFS()`)
usuwa oba pliki i zeruje `g_histShortSize`, `g_histLongSize`, `historiaPlikLiczbaLinii`,
`historiaDlugaPlikLiczbaLinii`, `historiaProbekOdZapisu`. Zero wyścigów.

## 4. Wilgotność — poprawka z tego samego wydania

`read_dht()` potrafi zwrócić `DHT_OK` z zerem, a warunek sprawdzał tylko `isnan()`.
Fałszywe **0,0 %** leciało do telemetrii i plików: zaniżało średnią archiwum
o ~5 punktów i dawało skoki **+100** na wykresie różnic (dokument 249, sekcja 0e).

```cpp
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f)
```

## 5. Aplikacja 0.31.64

| plik | zmiana |
|---|---|
| `core/PiecState.kt` | pole `histFs` + parsowanie `hist_fs` |
| `core/AppModel.kt` | `histFsState()`, `setHistFs()`, `clearHistFiles()` |
| `ui/screens/sheets/HistFs.kt` | **nowy ekran** „Zapis historii na flash" (`CheckRowEcho` + przycisk czyszczenia) |
| `ui/screens/Lists.kt` | kafelek w Ustawieniach |
| `ui/screens/Overlays.kt` | tytuł i dispatch arkusza |
| `app/build.gradle.kts` | `versionCode 31064`, `versionName "0.31.64"` |

## 6. Piec.html

Ten sam kafelek i ten sam ekran co w panelu centrali (kod 1:1), bo `Piec.html`
jest samodzielną kopią panelu. Wysyła te same komendy przez Firebase.

## 7. Zgodność

| firmware | aplikacja | efekt |
|---|---|---|
| 3.57.3 i starszy | 0.31.64 | ekran pokaże „brak danych" — **poprawnie**, brak pola `hist_fs` |
| 3.57.4 | 0.31.63 i starszy | przełącznik działa z Piec.html i panelu WWW; aplikacja go nie pokaże |

## 8. Sprawdzone / niesprawdzone

- **sprawdzone:** punkty zaczepienia trafione dokładnie raz (asercje w skrypcie),
  bilans nawiasów, brak zmian w formatach CSV i w ścieżce telemetrii,
- **niesprawdzone:** kompilacja — sandbox nie ma toolchainu ESP32. Jeśli `pio run`
  zgłosi błąd, wklej log i poprawię.

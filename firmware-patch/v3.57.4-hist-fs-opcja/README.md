# Firmware 3.57.4 — przełącznik zapisu historii na flash + poprawka wilgotności

Data: 2026-10-31 · baza: **3.57.3**
Zawartość: **pełne źródła** (podmień cały folder `src`, nie tylko `main_centrala.cpp`).

---

## Co nowego

### 1. Przełącznik „Zapis historii na flash"

- nowe pole w `/piec/status`: **`hist_fs`** — `1` = zapis włączony, `0` = wyłączony,
- ustawienie leży w **NVS** (namespace `hist`, klucz `fs_on`) — przeżywa restart i zanik prądu,
- **domyślnie WŁĄCZONE** — kto tego nie dotknie, ma zachowanie identyczne jak w 3.57.3.

Nowe komendy (Firebase, aplikacja, panel):

| komenda | efekt |
|---|---|
| `histfs_wl` | zapis historii na flash **włączony** |
| `histfs_wyl` | zapis historii na flash **wyłączony** |
| `histfs_czysc` | oba pliki zostaną **usunięte przy następnym starcie** |

**Co dokładnie odcina wyłączenie:** wyłącznie dopisywanie do `/hist_short.csv`
i `/hist_long.csv`. **Nie** dotyka:

- telemetrii do Firebase (`/piec/telemetry/1m/v1`) — osobna ścieżka, działa dalej,
- buforów w RAM (wykresy „6 h / 24 h" i archiwum czytane przez `/api/history*`),
- kolejki zapisu, WDT, nadzoru, TLS, OTA, żadnego innego ustawienia.

Po restarcie z wyłączonym zapisem: pliki zostają na dysku (stare dane), nic do nich
nie dopisuje. Bufor RAM odtwarza się z plików przy starcie — czyli wykresy dalej
mają historię sprzed wyłączenia.

### 2. Czyszczenie plików — dlaczego przy restarcie

Do plików historii pisze **wyłącznie** `historiaTaskFn` (Core 0) i tylko on mógłby
je bezpiecznie usunąć. Usunięcie pliku spod otwartego strumienia to prośba o błąd I/O,
dlatego `histfs_czysc` **ustawia znacznik w NVS**, a `historiaFsInicjalizujPrzyStarcie()`
(w `setup()`, przed `historiaWczytajZFS()`) usuwa oba pliki i zeruje liczniki.
Zero wyścigów, zero ryzyka przerwanego zapisu.

### 3. Wilgotność — 0,0 % traktowane jak błąd

Dotąd `read_dht()` potrafił zwrócić `DHT_OK` z zerem, a warunek sprawdzał tylko
`isnan()`. Efekt: fałszywe **0,0 %** leciało do telemetrii i do plików — zaniżało
średnią archiwum (~5 punktów) i dawało skoki **+100** na wykresie różnic.

```cpp
// było
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok)) {
// jest
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f) {
```

Zero wchodzi teraz w gałąź „błąd odczytu" — zostaje poprzednia dobra wartość.

---

## Gdzie to zobaczyć

| miejsce | jak |
|---|---|
| **panel centrali** (WWW ESP32) | Ustawienia → kafelek **„Zapis historii na flash"** |
| **Piec.html** | ten sam kafelek w Ustawieniach |
| **aplikacja Android 0.31.64** | Ustawienia → „Zapis historii na flash" |

Wszystkie trzy wysyłają te same komendy przez Firebase — jedno źródło prawdy.

---

## Wgranie

1. Podmień **cały folder `src`** na zawartość `src/` z tego ZIP-a (pliki `.h` też się zmieniły w poprzednich wersjach — bierz całość).
2. Zbuduj i wgraj przez OTA albo kablem.
3. W Ustawieniach sprawdź **`v3.57.4`**.
4. Wejdź w Ustawienia → „Zapis historii na flash". Ma pokazać **WŁĄCZONY** (domyślnie).

> **Nie wgrywaj 3.55.3–3.55.8** (wycofane, patrz `LINKI`).

---

## Czego nie zmieniono

Format CSV, kolejność i znaczenie kolumn, bufory RAM, kolejka zapisu, WDT,
nadzór z restartem, `fbApp.loop()` i timeouty TLS z 3.57.3, OTA, Telegram,
reguły Firebase, żadne inne ustawienie. Wszystkie wstawki z 3.57.2/3.57.3 są nietknięte.

---

## Sprawdzone automatycznie

- bilans nawiasów `{}`, `()`, `[]` — **delta względem 3.57.3 zbilansowana**
  (9/9, 71/71, 0/0) na całym pliku po odjęciu komentarzy i literałów,
- wszystkie punkty zaczepienia trafione dokładnie raz (skrypt zgłosiłby błąd),
- +179 linii: komentarz wersji, blok przełącznika, dwie bramki zapisu, wpis w statusie,
  trzy komendy, funkcja startowa, ekran w panelu WWW, poprawka wilgotności.

**Nie kompilowałem tego tutaj** — sandbox nie ma toolchainu ESP32. Sprawdź log
kompilacji; jeśli coś wyskoczy, wklej mi błąd i poprawię.

# Firmware 3.57.5 — przełącznik zapisu historii na flash (poprawiony po przeglądzie)

Data: 2026-10-31 · baza: **3.57.3** · zastępuje **3.57.4** (ta wersja nigdy nie poszła na piec)
Zawartość: **pełne źródła** — podmień cały folder `src`, nie tylko `main_centrala.cpp`.

---

## Trzy rzeczy poprawione względem 3.57.4

Przegląd kodu przed wgraniem wykazał trzy błędy. Wszystkie naprawione tutaj.

### 1. Czyszczenie plików idzie istniejącym kanałem A1, nie własnym kodem

W firmware **już był** audytowany mechanizm jednorazowego czyszczenia przy starcie:
`history_a190::Transition` + `history_a191::Storage`. Jeśli przy starcie leży
`/history_reset_a1.req` z treścią `RESET-HISTORY-A1\n`, transition:

1. usuwa **oba** pliki CSV,
2. sprawdza `stat`-em, że naprawdę ich nie ma,
3. tworzy `/history_reset_a1.done`,
4. zjada zadanie.

3.57.4 kasowało pliki własnym `LittleFS.remove()` po starcie i trzymało **drugi**
znacznik w NVS — dwa mechanizmy na jedną operację, bez potwierdzenia usunięcia.
Teraz `histfs_czysc` tylko **uzbraja kanał A1**:

| krok | co robi |
|---|---|
| 1 | usuwa stary `/history_reset_a1.done` (bez tego transition idzie ścieżką „już czyszczono" i **nie tknie plików**) |
| 2 | zapisuje `/history_reset_a1.req` = `RESET-HISTORY-A1\n` (17 bajtów, bez NUL — `Storage::request()` wymaga dokładnie tej długości) |
| 3 | sprawdza, że plik istnieje i ma pełną treść |

Efekt widoczny po restarcie: oba pliki znikają **przed pierwszym odczytem**.

### 2. Panel centrali i `Piec.html` w ogóle nie czytały pola `hist_fs`

Firmware wysyłał `hist_fs` w obu JSON-ach, ale oba panele parsują status po
**białej liście** pól (`flagKeys` / `numericRules` / `textKeys`). `hist_fs` jej nie było
— kafelek pokazywałby „Ta wersja firmware nie zgłasza tego ustawienia" **nawet na 3.57.5**.
Dodane do listy flag i do stanu początkowego; przełącznik dogania zmianę statusu
(przepisuje pole tylko wtedy, gdy wartość naprawdę się zmieniła, żeby nie cofać kliknięcia).

### 3. Wyłączenie zatrzymuje też opróżnianie kolejki

W 3.57.4 bramka obejmowała tylko **nowe** próbki — resztka czekająca w kolejce RAM
mogła jeszcze dolecieć na flash po wyłączeniu. Teraz bramka jest też w
`historiaFlushDoFS()` i `historiaDlugaFlushDoFS()`. Wyłączone = **żadnego dopisania**.
Resztka (max 4 KB) czeka w RAM i po ponownym włączeniu pojedzie jednym appendem.

---

## Co robi (bez zmian względem 3.57.4)

- nowe pole w `/piec/status`: **`hist_fs`** — `1` = zapis włączony, `0` = wyłączony,
- ustawienie w **NVS** (namespace `hist`, klucz `fs_on`) — przeżywa restart i zanik prądu,
- **domyślnie WŁĄCZONE** — kto tego nie dotknie, ma zachowanie jak w 3.57.3.

| komenda | efekt |
|---|---|
| `histfs_wl` | zapis historii na flash **włączony** |
| `histfs_wyl` | zapis historii na flash **wyłączony** |
| `histfs_czysc` | oba pliki **usunięte przy następnym starcie** |

**Czego nie odcina wyłączenie:** telemetrii do Firebase (`/piec/telemetry/1m/v1`),
buforów w RAM (wykresy „6 h / 24 h", `/api/history*`), kolejki, WDT, nadzoru,
TLS, OTA, Telegrama, formatu CSV. Odcina **wyłącznie** dopisywanie do
`/hist_short.csv` i `/hist_long.csv`.

### Dlaczego nie kasujemy plików „w locie"

Usunięcie pliku, do którego ktoś ma otwarty uchwyt i właśnie go synchronizuje,
potrafi zepsuć **inne** wpisy w katalogu — `littlefs-project/littlefs#1237`
(usunięty uchwyt trafia na następną parę metadanych i jego kolejny sync nadpisuje
obcy wpis; katalog zaczyna zwracać `LFS_ERR_CORRUPT`). U nas dokładnie taki układ:
`historiaTaskFn` dopisuje co 5 próbek. Dlatego kasowanie jest tylko przy starcie,
zanim ktokolwiek otworzy te pliki.

### Wilgotność — 0,0 % traktowane jak błąd

```cpp
// było
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok)) {
// jest
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f) {
```

DHT22 potrafi zwrócić ramkę samych zer z **poprawną sumą kontrolną** (0+0+0+0=0),
więc biblioteka melduje `DHT_OK`, a w danych ląduje 0,0 %. Zero wchodzi teraz
w gałąź „błąd odczytu" — zostaje poprzednia dobra wartość.

---

## Gdzie to zobaczyć

| miejsce | jak |
|---|---|
| **panel centrali** (WWW ESP32) | Ustawienia → kafelek **„Zapis historii na flash"** |
| **Piec.html** | ten sam kafelek w Ustawieniach |
| **aplikacja Android 0.31.64** | Ustawienia → „Zapis historii na flash" |

Wszystkie trzy wysyłają te same komendy przez Firebase — jedno źródło prawdy.
Aplikacja **0.31.64 zostaje bez zmian** (protokół ten sam); jej podpowiedź
„potrzebna 3.57.4" znaczy „3.57.4 albo nowsza".

---

## Wgranie

1. Podmień **cały folder `src`** na zawartość `src/` z tego ZIP-a.
2. Zbuduj i wgraj przez OTA albo kablem.
3. W Ustawieniach sprawdź **`v3.57.5`**.
4. Wejdź w Ustawienia → „Zapis historii na flash" — ma pokazać **WŁĄCZONY**.

> **Nie wgrywaj 3.55.3–3.55.8** (wycofane) ani **3.57.4** (ta wersja).

---

## Sprawdzone automatycznie

Test `narzedzia/testy-firmware/test-hist-fs.js` — **27/27 OK**:

- treść i długość zadania A1 zgodna z `Storage.h` (`RESET-HISTORY-A1\n`, 17 B),
- ścieżka `.done` zgodna z `EspVfsCalls.h`, kolejność „najpierw `.done`, potem `.req`",
- `hist_fs` w obu JSON-ach i w listach flag **obu** paneli,
- brak starego znacznika NVS `czysc`, brak `LittleFS.remove` na plikach historii,
- bilans `{}`, `()`, `[]` w `main_centrala.cpp` po odjęciu literałów i komentarzy.

**Nie kompilowałem tego tutaj** — sandbox nie ma toolchainu ESP32. Jeśli `pio run`
coś zgłosi, wklej log, poprawię.

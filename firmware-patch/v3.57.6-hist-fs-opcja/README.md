# Firmware 3.57.6 — przełącznik zapisu historii na flash (po symulacjach)

Data: 2026-10-31 · baza: **3.57.3** · zastępuje **3.57.5** (ta nigdy nie poszła na piec)
Zawartość: **pełne źródła** — podmień cały folder `src`, nie tylko `main_centrala.cpp`.

---

## Co wykryły symulacje przed wgraniem

Symulator startu uruchomiony na **prawdziwych klasach z repo** (`HistoryBoot.h`,
`HistoryStartup.h`, `Storage.h`, `Transition.h`) pokazał dwa przypadki, w których
**zanik prądu w złej sekundzie potrafi zatrzymać historię na zawsze**:

`Transition` czyta `/history_reset_a1.req` i każde odstępstwo (pusty plik, 9 z 17
bajtów, inna treść) traktuje jako błąd → `history194::Startup` nie wstaje → historia
nie działa **do końca życia urządzenia** (edytor plików w panelu tę ścieżkę blokuje,
więc nie da się jej naprawić z aplikacji).

| # | przypadek | co się działo w 3.57.5 | co jest w 3.57.6 |
|---|---|---|---|
| 1 | zanik prądu **w trakcie** zapisu `.req` | `open(..., "w")` najpierw skracało plik do zera, potem wpisywało 17 B → zostawało 0 albo 9 B → **historia martwa** | zapis do `.req.tmp` → odczyt kontrolny (17 B, `memcmp`) → `rename()` na `.req`. Rename w LittleFS jest atomowy: plik jest w całości albo go nie ma |
| 2 | uszkodzony `.req` **już leży** na dysku | to samo → **historia martwa** | `setup()` przenosi go do `.req.bad` **przed** `historiaBoot.start()`. Historia startuje, pliki CSV nietknięte (uszkodzone zadanie to nie zadanie czyszczenia) |

Dodatkowo dwie rzeczy mniejsze:

| # | co | poprawka |
|---|---|---|
| 3 | po ponownym **włączeniu** zapisu resztka z kolejki czekała do 5 próbek (~5 min) | `histfs_wl` zgłasza natychmiastowy flush |
| 4 | w panelu przełącznik **nie wracał**, gdy komenda została odrzucona | `.then(ok => ...)` cofa checkbox do stanu z centrali |

## Trzy poprawki z przeglądu 3.57.4 (bez zmian)

1. **Czyszczenie idzie istniejącym kanałem A1** — `histfs_czysc` usuwa stary
   `/history_reset_a1.done` i składa `/history_reset_a1.req` = `RESET-HISTORY-A1\n`
   (17 B). `history_a190::Transition` przy starcie usuwa oba CSV, sprawdza
   `stat`-em, że ich nie ma, tworzy `.done` i zjada zadanie. Żadnego własnego
   `LittleFS.remove()` na plikach historii.
2. **`hist_fs` w panelu i Piec.html** — oba parsują status po białej liście pól;
   pole jest w `flagKeys` i w stanie początkowym.
3. **Bramka obejmuje opróżnianie kolejki** — `historiaFlushDoFS()` i
   `historiaDlugaFlushDoFS()` też sprawdzają przełącznik. Wyłączone = **ani jednej
   linii** na flash; resztka (≤ 4 KB) czeka w RAM i po włączeniu jedzie jednym appendem.

---

## Co robi

- nowe pole w `/piec/status` (i `/api/status`): **`hist_fs`** — `1` = zapis włączony, `0` = wyłączony,
- ustawienie w **NVS** (namespace `hist`, klucz `fs_on`) — przeżywa restart i zanik prądu,
- **domyślnie WŁĄCZONE** — kto tego nie dotknie, ma zachowanie jak w 3.57.3.

| komenda | efekt |
|---|---|
| `histfs_wl` | zapis historii na flash **włączony** (+ natychmiastowy flush resztki) |
| `histfs_wyl` | zapis historii na flash **wyłączony** |
| `histfs_czysc` | oba pliki **usunięte przy następnym starcie** |

**Czego nie odcina wyłączenie:** telemetrii do Firebase (`/piec/telemetry/1m/v1`),
buforów w RAM (wykresy „6 h / 24 h”, `/api/history*`), kolejki, WDT, nadzoru,
TLS, OTA, Telegrama, formatu CSV. Odcina **wyłącznie** dopisywanie do
`/hist_short.csv` i `/hist_long.csv`.

### Dlaczego nie kasujemy plików „w locie”

Usunięcie pliku, do którego ktoś ma otwarty uchwyt i właśnie go synchronizuje,
potrafi zepsuć **inne** wpisy w katalogu — `littlefs-project/littlefs#1237`.
U nas dokładnie taki układ: `historiaTaskFn` dopisuje co 5 próbek. Dlatego
kasowanie jest tylko przy starcie, zanim ktokolwiek otworzy te pliki.

### Wilgotność — 0,0 % traktowane jak błąd

```cpp
// było
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok)) {
// jest
if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f) {
```

DHT22 potrafi zwrócić ramkę samych zer z **poprawną sumą kontrolną**, więc
biblioteka melduje `DHT_OK`, a w danych ląduje 0,0 %. Zero wchodzi teraz w gałąź
„błąd odczytu” — zostaje poprzednia dobra wartość.

> **Ograniczenie, o którym trzeba wiedzieć:** jeśli czujnik jest **martwy od
> rozruchu**, „poprzednia dobra wartość” to nadal 0,0 i takie zera będą w plikach
> do wymiany czujnika. Poprawka działa przy zerach **przerywających** normalny
> pomiar. Pokazywanie „—” zamiast 0,0 wymaga osobnego pola w statusie i nowej
> wersji aplikacji — do decyzji.

---

## Gdzie to zobaczyć

| miejsce | jak |
|---|---|
| **panel centrali** (WWW ESP32) | Ustawienia → kafelek **„Zapis historii na flash”** |
| **Piec.html** | ten sam kafelek w Ustawieniach |
| **aplikacja Android 0.31.64** | Ustawienia → „Zapis historii na flash” |

Wszystkie trzy wysyłają te same komendy przez Firebase — jedno źródło prawdy.
Aplikacja **0.31.64 zostaje bez zmian** (protokół ten sam).

---

## Wgranie

1. Podmień **cały folder `src`** na zawartość `src/` z tego ZIP-a.
2. Zbuduj i wgraj przez OTA albo kablem.
3. W Ustawieniach sprawdź **`v3.57.6`**.
4. Ustawienia → „Zapis historii na flash” — ma pokazać **WŁĄCZONY**.
5. Opcjonalnie: wyłącz, odczekaj 5 min, sprawdź, że `hist_short.csv` nie rośnie;
   włącz z powrotem.

> **Nie wgrywaj 3.55.3–3.55.8** (wycofane), **3.57.4 ani 3.57.5** (zastąpione).

---

## Sprawdzone automatycznie

```
test-hist-fs.js         33 OK / 0 FAIL   (umowa z Storage.h, panele, bilans nawiasów)
symulacja C++           80 OK / 0 FAIL   (17 scenariuszy startu + model bramek)
panel w jsdom           26 OK / 0 FAIL   (parsowanie statusu, klik, ACK, brak ACK)
```

Uruchomienie jednym poleceniem: `bash narzedzia/symulacje-firmware/run-all.sh`

**Nie kompilowałem tego tutaj** — sandbox nie ma toolchainu ESP32. Jeśli `pio run`
coś zgłosi, wklej log, poprawię.

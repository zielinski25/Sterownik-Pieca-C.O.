# Firmware v3.56.8 — „Ogrz. śr." po restarcie + detektor luk

Data: 2026-10-29
Baza: v3.56.7 (czyli zachowanie v3.56.5)
Plik: `main_centrala.cpp`

---

## 1. Co to naprawia

### Problem widoczny na wykresie

Linia **„Ogrz. śr."** ma dziurę przez **~20 minut po każdym restarcie pieca**.
Zmierzone na danych z 29.10: trzy przebiegi, każdy dokładnie 20 próbek,
każdy startujący w chwili uruchomienia pieca.

**To nie jest utrata danych.** Kanał po prostu nie ma czym się wypełnić.

### Dlaczego tak było

Funkcja `obliczanie_srednia()` uznaje piec za wygasły po `Czas_OFF` minutach
od zmiennej `ostatniRazPompaPracowalaMs`. Ta zmienna jest `static`, więc
**po każdym restarcie ma 0**. Piec, który stoi od wczoraj, musi więc odczekać
pełne 20 minut, żeby „zauważyć", że jest zimny.

### Co zmieniłem

Przy **zaniku pompy** zapisuję czas (unix) do NVS pod kluczem `srOstPompa`.
Po restarcie odejmuję ten czas od zegara i cofam licznik o zmierzony odstęp.
Jeśli piec stoi dłużej niż `Czas_OFF` — warunek „wygasł" jest spełniony
**od pierwszej próbki**.

**Efekt:** kanał „Ogrz. śr." ma wartość natychmiast po restarcie. Dziura znika.

**Koszt:** jeden zapis do NVS na cykl pracy pompy. Pojedyncze zapisy — nie
ma zużycia pamięci.

---

## 2. Co dodaje (tylko diagnostyka — nic nie zmienia w działaniu)

**Detektor luk.** Po każdej udanej wysyłce sprawdzam, czy kolejny numer `seq`
w kolejce jest dokładnie o 1 większy od potwierdzonego. Jeśli nie — między
jednym a drugim zniknęły próbki. Liczę je i zapisuję **konkretne numery**.

Po co: audyt 29 dni pokazał **65 pojedynczych strat**, dla których w kodzie
nie ma żadnej ścieżki. Ten detektor powie wprost, kiedy i ile numerów
przeskoczyło.

Nowe liczniki — **tylko w logu `HEALTH-A`** (czyli w `/logi` albo Telegram `/logi`).

> **SPROSTOWANIE (30.10).** Wcześniej ten plik twierdził, że liczniki są też
> w JSON `/api/status`. **To nieprawda** — sprawdzone w kodzie: każde z pól
> `telemetry_*` występuje dokładnie raz, w linii HEALTH-A (linie 47119–47124).
> `/api/status` ich nie zawiera. Do odczytu potrzebny jest więc log, nie status.



| licznik | znaczenie |
|---|---|
| `telemetry_sent` | ile próbek faktycznie poszło do chmury |
| `telemetry_ring` | ile czeka w kolejce |
| `telemetry_gap_events` | ile razy wykryto przerwę w numeracji |
| `telemetry_gap_samples` | ile próbek łącznie przeskoczono |

---

## 3. Wersja podbita?

**TAK.** `v3.56.7` → `v3.56.8`. Bez tego OTA odrzuci plik jako starszy.

---

## 4. Czego ta wersja NIE zmienia

- Format JSON, ścieżki RTDB, interwał 60 s, limity payloadu
- `TELEMETRY_BATCH_MAX_RECORDS`, `MAX_ATTEMPTS`, format stron Spool V2
- Wszystkie trasy HTTP, Telegram, panel, historia, solar
- Logika wygładzania średniej, progi, tryby Wybor 1/2/3
- Zapis `tempSrednia` przy zboczu OFF→ON

Ryzyko: **małe**. Zmiana 1 dotyka jednego licznika czasu w jednej funkcji.
Zmiana 2 to wyłącznie dodanie liczników i logu.

---

## 5. Co będzie dalej

Po **jednym dniu** pracy wyślij `/logi` (`/status` nie zawiera tych liczników). Z nowych liczników
odczytam, gdzie ginie ta jedna próbka. Wtedy dopiero — na dowodzie, nie na
hipotezie — zrobimy ewentualną poprawkę mechanizmu paczek.

---

## 6. Zawartość paczki

```
main_centrala.cpp                         <- plik do src\
main_centrala-v3.56.7-to-v3.56.8.patch    <- ten sam plik jako łatka
README.md                                 <- ten plik
CZYTAJ-NAJPIERW.txt                       <- instrukcja krok po kroku
SHA256SUMS.txt                            <- sumy kontrolne
*.h (14 plików)                           <- BEZ ZMIAN wobec v3.56.7
secrets.h.example                         <- wzór, NIE nadpisuj swojego
```

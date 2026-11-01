# Firmware v3.57.1 — szybsze KOMENDY, status bez zmian

Data: 2026-10-30
Baza: v3.57.0
Plik do podmiany: **`main_centrala.cpp`** (jeden plik)
Pliki `.h`: **bez zmian** (te same co 3.57.0 — są w paczce tylko dla kompletności)

---

## 1. Co jest celem

Ustalenie z 2026-10-30: **chodzi o komendy i potwierdzenie**, nie o świeżość temperatur.

Dlatego tryb „podgląd panelu" (aplikacja otwarta) przyspiesza **wyłącznie odczyt
skrzynki z poleceniami** — z **7 s na 2 s**. Wysyłka statusu `/piec/status`
zostaje **bez zmian: co 15 s** (w v3.57.0 było 5 s — to wycofane).

## 2. Co się zmieniło wobec v3.57.0

| element | v3.57.0 | **v3.57.1** |
|---|---|---|
| odczyt komend w podglądzie | 2 s | **2 s (bez zmian)** |
| wysyłka statusu w podglądzie | 5 s | **15 s — jak w normie** |
| krok pętli w podglądzie | 1 s | 1 s (potrzebny, by bramka 2 s działała) |
| heartbeat `podglad_wl` / `podglad_wyl`, okno 90 s | tak | tak (bez zmian) |
| turbo po prawdziwej komendzie (1 s/1 s przez 60 s) | tak | tak (bez zmian, od v3.12.0) |

Usunięta stała `FB_PODGLAD_SEND_INTERVAL_MS` i jej użycie — bramka statusu
wybiera teraz tylko: turbo (1 s) albo norma (15 s).

## 3. Trzy stopnie — stan końcowy

| tryb | status `/piec/status` | odczyt `/piec/cmd` | krok pętli |
|---|---|---|---|
| turbo (60 s po prawdziwej komendzie) | 1 s | 1 s | 1 s |
| **podgląd (okno 90 s od `podglad_wl`)** | **15 s (bez zmian)** | **2 s** | 1 s |
| norma (nikt nie patrzy) | 15 s | 7 s | 3 s |

## 4. Efekt dla użytkownika

| | przed 3.57.x | po 3.57.1 + aplikacja 0.31.57 |
|---|---|---|
| klik → centrala odczytała polecenie | do 7 s | **do 2 s** (w turbo 1 s) |
| klik → „potwierdzone" w aplikacji | do ~8 s | **~1,3–2,3 s** |
| świeżość temperatur | do 15 s | **do 15 s (bez zmian)** |

## 5. Koszt

Przy otwartej aplikacji centrala wykonuje **~1800 dodatkowych odczytów**
`/piec/cmd` na godzinę (samo „czy jest polecenie?" — odpowiedź to `null`).
**Zero dodatkowych zapisów statusu.** Po zamknięciu aplikacji (albo po 90 s
bez heartbeatu) wszystko wraca do 7 s.

## 6. Czego NIE zmienia

Format `/piec/cmd`, `/piec/status`, `/piec/ack`; logika pieca, pomp, serwa, alarmów,
historii, Telegrama; średnia „Ogrz. śr." z 3.56.9; tryb turbo i jego okno 60 s.

## 7. Pliki w paczce

- `main_centrala.cpp` — **pełny plik do podmiany**
- `main_centrala-v3.57.0-to-v3.57.1.patch` — sama różnica wobec 3.57.0 (do przeglądu)
- `README.md` (ten plik), `CZYTAJ-NAJPIERW.txt`, `SHA256SUMS.txt`
- pozostałe `.h` + `secrets.h.example` — bez zmian

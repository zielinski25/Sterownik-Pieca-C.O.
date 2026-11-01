# Firmware v3.57.0 — „podgląd panelu": komendy docierają w ~2 s, nie w ~7 s

Data: 2026-10-30
Baza: v3.56.9
Plik do podmiany: **`main_centrala.cpp`** (jeden plik)
Pliki `.h`: **bez zmian** (te same co 3.56.9 — są w paczce tylko dla kompletności)

---

## 1. Co to naprawia

Centrala czyta `/piec/cmd` **co 7 s** (`FB_CMD_INTERVAL_MS = 7000UL`). Tryb turbo
(1 s/1 s) startował **dopiero po odczytaniu** payloadu — więc pierwsza komenda po
dłuższej ciszy czekała na odbiór **do 7 s**, mimo że telefon był otwarty i ktoś
właśnie klikał. Potwierdzenie (ACK) wracało po tym czasie, nie po ~0,3 s.

Firmware nie miał żadnego sygnału „ktoś patrzy na panel" — znał tylko ciszę
i pojedynczą komendę.

## 2. Co doszło

Dwie nowe komendy (nic nie przełączają, nie zmieniają stanu pieca):

| komenda | znaczenie |
|---|---|
| `podglad_wl` | heartbeat „aplikacja otwarta" — przesuwa okno podglądu o **90 s** |
| `podglad_wyl` | jawne zamknięcie panelu — natychmiastowy powrót do normy |

Aplikacja 0.31.57 wysyła `podglad_wl` od razu po wejściu na pierwszy plan
i potem co 50 s. Gdy zejdzie w tło — `podglad_wyl`. Gdy proces zostanie zabity,
okno samo wygaśnie po 90 s (nie ma stanu, z którego trzeba by centralę wyprowadzać).

## 3. Trzy stopnie szybkości

| tryb | status `/piec/status` | odczyt `/piec/cmd` | krok pętli |
|---|---|---|---|
| **turbo** (60 s po prawdziwej komendzie) | 1 s | 1 s | 1 s |
| **podgląd** (okno 90 s od heartbeatu) | **5 s** | **2 s** | 1 s |
| norma (nikt nie patrzy) | 15 s | 7 s | 3 s |

Turbo zawsze wygrywa z podglądem. Heartbeat podglądu **nie wrzuca** centrali
w pełne turbo 1 s/1 s — jeśli turbo uruchomił właśnie ten odczyt heartbeatu
(a nie wcześniejsza prawdziwa komenda), jest cofane.

## 4. Efekt (koniec z końcem, aplikacja 0.31.57)

| etap | przed | po |
|---|---|---|
| odczyt komendy przez centralę | do 7 s | **do 2 s** (w turbo 1 s) |
| ACK zapisany przez centralę | +~0,2 s | +~0,2 s |
| aplikacja zauważa ACK | do 1 s | **do 0,3 s** |
| **razem: klik → „potwierdzone"** | **do ~8 s** | **~1,3–2,5 s** |
| świeżość statusu przy otwartej aplikacji | do 15 s | **5 s** (turbo 1 s) |

## 5. Koszt

Przy otwartej aplikacji: **+~1800 odczytów** `/piec/cmd` i **+~720 zapisów**
`/piec/status` na godzinę wobec normy. Po zamknięciu aplikacji — **zero różnicy**
wobec 3.56.9 (po 90 s centrala wraca na 15 s/7 s).

## 6. Czego NIE zmienia

- format `/piec/cmd`, `/piec/status`, `/piec/ack` — bez zmian,
- tryb turbo i jego okno 60 s — bez zmian,
- średnia „Ogrz. śr." z 3.56.9 — bez zmian,
- logika pieca, pomp, serwa, alarmów, historii, Telegrama — bez zmian.

## 7. Zgodność z aplikacją

| aplikacja | firmware 3.56.x i starszy | firmware 3.57.0 |
|---|---|---|
| 0.31.56 i starsza | jak dotąd | działa (2 s polling statusu wyłapie szybsze publikacje) |
| **0.31.57** | działa: heartbeat i tak uruchamia turbo 1 s/1 s, ale w logu centrali pojawia się `nieznana_komenda` co ~50 s | **pełny efekt: 2 s odczyt komend, 5 s status, zero śmieci w logu** |

Kolejność nie ma znaczenia — można wgrać samo firmware albo samą aplikację.

## 8. Pliki w paczce

- `main_centrala.cpp` — **pełny plik do podmiany** (2 393 043 B)
- `main_centrala-v3.56.9-to-v3.57.0.patch` — sama różnica (141 linii) do przeglądu
- `README.md` (ten plik), `CZYTAJ-NAJPIERW.txt`
- `SHA256SUMS.txt`
- pozostałe `.h` + `secrets.h.example` — bez zmian wobec 3.56.9

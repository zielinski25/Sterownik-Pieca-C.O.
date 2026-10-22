# Android v0.31.22 — „Bufor offline" + procenty na kafelku SERWO

Data: 2026-10-22
Baza: v0.31.19 (drzewo użytkownika)
Wynik: **v0.31.22 (3028)**
Wymaga: **firmware v3.54.0** — bez niego przełącznik jest nieaktywny, ale nic się nie psuje.

## Co zawiera ta wersja (kumulatywnie od 0.31.19)

| # | zmiana | gdzie |
|---|---|---|
| F1 | dzienny cache telemetrii | `core/TelemetryCache.kt` (nowy), `RtdbClient.kt` |
| F2 | historia tylko na Wykresach | `AppModel.kt` |
| F3 | anulowanie pobrania + 60 s blokady | `AppModel.kt` |
| F4 | błędy dni widoczne w statusie | `RtdbClient.kt`, `Charts.kt` |
| F5 | TTL flagi awaryjnej 10 min | `RtdbClient.kt` |
| F9 | status mówi, skąd dane | `Charts.kt` |
| T | terminal DLOG: stop po 5 min bezczynności | `AppModel.kt`, `sheets/System.kt` |
| P | kafelek SERWO w procentach | `ui/screens/Dashboard.kt` |
| **B** | **Ustawienia → „Bufor offline"** | `PiecState.kt`, `AppModel.kt`, `Lists.kt`, `Overlays.kt`, `sheets/Offline.kt` (nowy) |

## [B] Bufor offline — co dokładnie zrobiono

| plik | zmiana |
|---|---|
| `core/PiecState.kt` | pola `spoolRt`, `spoolPages`, `spoolSpill`, `spoolReplay` + parsowanie `spool_*` |
| `core/AppModel.kt` | `offlineBufferState(): Boolean?` (null = stary firmware), `setOfflineBuffer(on)` |
| `ui/screens/Lists.kt` | kafelek menu „Bufor offline" w Ustawieniach |
| `ui/screens/Overlays.kt` | tytuł arkusza + trasa `bufor` |
| `ui/screens/sheets/Offline.kt` | **nowy** arkusz: przełącznik, liczniki, ostrzeżenia |
| `app/build.gradle.kts` | 3027 → **3028**, `v0.31.21` → **`v0.31.22`** |

Zasada: stan czytamy **wyłącznie ze statusu** (`spool_rt`), nigdy z tego, co
użytkownik kliknął. Po kliknięciu leci `spoolrt_wl` / `spoolrt_wyl`, potem ACK,
a UI zmienia się dopiero po świeżym snapshocie — tak samo jak przy pozostałych
poleceniach.

Gdy firmware jest starszy (brak `spool_rt`): przełącznik nieaktywny z napisem
„Ta wersja firmware nie zgłasza tego ustawienia". Zero crashy, zero zgadywania.

## Paczka

| | wartość |
|---|---|
| ZIP | `SterownikCO-Android-v0.31.22-BUFOR-OFFLINE.zip` |
| rozmiar | 108 500 B |
| sha256 | `1bb4f02ec9aa575b474ef6961684b530384b807a4fb8808d1dac3f6f581bfbe8` |
| wpisy | 15 (README + diff + 13 plików źródłowych) |
| pliki do nadpisania | 10 |
| pliki nowe | 3 (`TelemetryCache.kt`, `Offline.kt`, `TelemetryCacheTest.kt`) |

## Czego nie sprawdzono

Brak JDK / Android SDK → `assembleDebug` i testy muszą pójść na laptopie.
Wykonane: `kcheck.py` na wszystkich zmienionych plikach (balans nawiasów
i stringów) — OK; kontrola ZIP-a przez rozpakowanie i `cmp` 13 plików — OK.

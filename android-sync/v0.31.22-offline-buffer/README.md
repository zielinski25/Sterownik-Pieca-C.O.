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

## Gdy build pada na `Unresolved reference` — pełne źródła

Mała paczka (13 plików) wymaga drzewa **v0.31.18/v0.31.19**. Na starszym
projekcie kompilator zgłosi `ServoPositionPreview`, `ServoPositionAxis`,
`AlarmSoundSync`, `commandChangesDashboardState` — brakuje trzech plików,
których nakładka nie zawiera.

Wtedy: **`SterownikCO-Android-v0.31.22-PELNE-ZRODLA.zip`** (1 119 837 B,
140 wpisów, cały projekt) — rozpakować do nowego folderu i zbudować.
Szczegóły: `README-PELNE-ZRODLA.md`.

## Build potwierdzony (2026-10-22)

Pełne źródła zbudowane na maszynie użytkownika bez błędów:

```
BUILD SUCCESSFUL in 1m 17s
APK: app\build\outputs\apk\debug\app-debug.apk  (12 889 996 B)
```

Gradle 8.9 (wrapper) · AGP 8.7.3 · Kotlin 2.0.21 · JDK 17.0.20.1 Temurin ·
compileSdk 35 · `local.properties` z `sdk.dir=C:/Users/<Ty>/AppData/Local/Android/Sdk`.

Ostrzeżenia kompilatora są w plikach spoza tej paczki (deprecations w
`ChartEngine.kt`, `Weather.kt`, `SvgPath.kt`, `Theme.kt`) — nie blokują builda
i nie dotyczą zmian 0.31.20–0.31.22.

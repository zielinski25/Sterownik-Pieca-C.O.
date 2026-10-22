# v0.31.20 — historia telemetrii: cache dzienny i koniec z cichymi dziurami

Paczka naprawcza dla drzewa **0.31.19** (= `SterownikCO-Android-v0.31.18-build-20261016-094945\android`
+ nakładka `v3.50.0-ALARM-SOUND-SYNC`). Nie jest to łatka na repo — repo ma źródła 0.31.11,
a 0.31.18/0.31.19 nigdy do niego nie trafiły.

- **Wersja docelowa:** `versionName = "v0.31.20"`, `versionCode = 3026`
- **ZIP:** `SterownikCO-Android-v0.31.20-HISTORIA-CACHE.zip` — 69 652 B,
  sha256 `6eab3b3a7969ace4b81ef87a0a842a4197586cc704459fe0484c86bd3a77ef77`
- **Drive:** `1V2yFS4VdA_rqWp5vVSVfegmxjaGpbyN0`

## Co zmieniono

| plik | rodzaj | rola |
|---|---|---|
| `app/build.gradle.kts` | zmieniony | 3025 → 3026, `v0.31.19` → `v0.31.20` |
| `core/TelemetryCache.kt` | **nowy** | dzienny cache JSON w `filesDir/telemetry/v1/` |
| `core/RtdbClient.kt` | zmieniony | `fetchTelemetry` zwraca `HistoryFetch`, czyta/zapisuje cache, dociąga przyrostowo, raportuje błędy dni |
| `core/Telemetry.kt` | zmieniony | `data class HistoryFetch` (punkty + liczniki dni) |
| `core/AppModel.kt` | zmieniony | `loadRange(sec, force)`, anulowanie `Job`, 60 s blokady, historia tylko na Wykresach, `historyStatusText` |
| `ui/screens/Charts.kt` | zmieniony | przycisk odświeżania woła `force = true` |
| `test/.../TelemetryCacheTest.kt` | **nowy** | 8 testów JVM, bez Androida i sieci |

## Mapowanie na znaleziska z raportu

| id | znalezisko | status w 0.31.20 |
|---|---|---|
| F1 | brak cache — każde wejście pobiera cały zakres | **naprawione** (cache dzienny) |
| F2 | `onResume()` ładuje historię bezwarunkowo | **naprawione** (`if (page == 1)`) |
| F3 | brak anulowania poprzedniego pobrania | **naprawione** (`historyJob?.cancel()`) |
| F4 | cichy `catch` per doba w `RtdbClient` | **naprawione** (`daysFailed` / `daysStale`) |
| F5 | `historyIndexFallback` nie resetuje się do końca procesu | **naprawione** (TTL 10 min, koniec z `contains("400")`) |
| F6 | brak downsamplingu (43 200 pkt × 14 serii) | **otwarte** — osobny etap |
| F7 | `Cache-Control: no-store` | bez zmian (brak `OkHttp.Cache`, `no-store` chroni tylko przed pośrednikami) |
| F8 | `/solar_daily` nieczytane w aplikacji | **otwarte** — pytanie 2 w sekcji 8 |
| F9 | brak informacji o źródle danych | **naprawione** (status: ile dni z pamięci / pobranych / brak) |
| F10 | `System.kt` pokazuje `BuildConfig.VERSION_NAME` | bez zmian, ale numer podbity → odróżnisz APK |

## Zasady ruchu sieciowego po zmianie

1. dzień **bez pliku** w cache → pobranie **całego** dnia (nie wycinka zakresu) i zapis,
2. dzień **bieżący (UTC)** → tylko rekordy nowsze od `maxTs` z pliku,
3. dzień **wczorajszy** → jedno pełne domknięcie na proces, **scalanie**, nigdy podmiana,
4. dzień **zamknięty** → zero sieci, tylko plik.

Skutek: 30 dni ≈ 8,1 MB → ~0,3 MB przy powrocie; przełączanie zakładek → 0.

## Weryfikacja

- `python3 /tmp/kcheck.py` — balans nawiasów/stringów w 5 plikach: OK
- port polityki cache do Pythona, 10 scenariuszy (zimny start, przyrost, awaria sieci,
  prune, roundtrip NaN, 24 h → 7 dni): **wszystko OK**
- **Nie skompilowano** — w sandboxie nie ma JDK ani Android SDK (potwierdzone w `README.md`
  drzewa Androida). Kompilacja i `testDebugUnitTest` po stronie użytkownika.

## Czego świadomie nie zrobiono

- downsampling wykresu (F6) — zmienia wygląd serii, wymaga decyzji o min/max,
- odczyt `/solar_daily` (F8) — czeka na decyzję, czy archiwum solarne ma być w aplikacji,
- czyszczenie cache z UI — wystarcza prune 45 dni + limit 24 MB.

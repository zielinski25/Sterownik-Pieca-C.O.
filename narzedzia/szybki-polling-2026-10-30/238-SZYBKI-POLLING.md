# 238 — szybsze odświeżanie przy otwartej aplikacji (0.31.56)

Data: 2026-10-30. Wersja aplikacji: **0.31.56** (`versionCode 31056`).
Firmware centrali: **bez zmian** (zostaje 3.56.8/3.56.9 — ta zmiana niczego w centrali nie rusza).

## Co powiedziałeś

> „jeśli aplikacja będzie odpalona na pierwszym planie to wtedy Firebase będzie
> odpytywało częściej… teraz jest zrobione tak, że odpytuje, bo jeśli komenda
> jest jakaś wydana to wtedy przez jakiś czas gotuje szybko"

Wybrany wariant: **A — tylko aplikacja, bez zmian w centrali.**

## Jak było naprawdę (sprawdzone w kodzie, nie z opisów)

| gdzie | co robiło | plik / linia |
|---|---|---|
| Aplikacja, pierwszy plan | pyta `/piec/status` **co 8 s** | `core/AppModel.kt:261` (stara linia) |
| Aplikacja, tło | **0 zapytań** — `onPause` kasuje `jobs` | `core/AppModel.kt` `onPause()` |
| Aplikacja, tło — alarmy | 1 zapytanie na **60 s** | `service/AlarmMonitorService.kt:398` |
| Centrala, normalnie | publikuje status **co 15 s**, czyta komendy **co 7 s**, pętla 3 s | `main_centrala.cpp:43575–43578` |
| Centrala, po komendzie | **turbo: 1 s / 1 s przez 60 s** | `main_centrala.cpp:42863–42865`, `43432` |

Czyli „gotuje szybko po komendzie" to **firmware**, nie aplikacja: każda poprawna
komenda z panelu przesuwa `fbTurboUntilMs = millis() + 60000`.

## Co zmieniłem (2 pliki + wersja)

| plik | zmiana |
|---|---|
| `core/AppModel.kt` | nowa stała `FOREGROUND_POLL_MS = 2_000L`; pętla `delay(8000)` → `delay(FOREGROUND_POLL_MS)`; komentarze z „8 s" → „2 s" |
| `MainActivity.kt` | komentarz nagłówka: „8 s odpytywanie" → „2 s odpytywanie" |
| `app/build.gradle.kts` | `versionCode 31055 → 31056`, `versionName "0.31.55" → "0.31.56"` |

W tle **nic się nie zmieniło**: dalej zero zapytań z UI, alarmy co 60 s.
Pobieranie historii dobowej z chmury ma własny limit 5 min (`refreshSolarFromCloud`),
więc szybszy polling go nie mnoży. Pogoda: dalej co 15 min.

## Co to daje, a czego nie daje

| sytuacja | przed 0.31.56 | po 0.31.56 |
|---|---|---|
| klik w kafelek → widać skutek | do 8 s po ACK | do 2 s po ACK |
| bezczynność: jak stare są dane | do 15 s (zapis) + do 8 s (odbiór) ≈ 23 s | do 15 s (zapis) + do 2 s (odbiór) ≈ 17 s |
| aplikacja w tle | 0 zapytań | 0 zapytań |
| ruch na godzinę (otwarta app) | 450 odczytów | 1 800 odczytów |

**Świeżości przy bezczynności to nie naprawia** i naprawić nie może: telefon
przeczyta tylko to, co centrala już zapisała, a centrala pisze co 15 s.
Gdybyś chciał świeżość 3–5 s *zawsze*, trzeba wariantu B (firmware 3.57.0:
gdy aplikacja otwarta, centrala nadaje co 3 s). Wariant C (1 s) to ~86 400
zapisów na dobę — największe obciążenie ESP32.

Ruch: przy ~2 kB na dokument statusu 1 800 odczytów/h ≈ 3,6 MB/h. Przy dwóch
otwartych telefonach — podwójnie (każdy pyta własnym tempem).

## Jak sprawdzić

1. Ustawienia → na dole **`v0.31.56`**.
2. Otwórz aplikację, kliknij np. pompę: kafelek/stan zmienia się w **~1–2 s**
   (centrala w turbo nadaje co 1 s, telefon czyta co 2 s).
3. Zostaw aplikację w tle (np. ekran główny) — w logu centrali nie ma ruchu
   z telefonu; alarmy dalej przychodzą (AlarmMonitorService, 60 s).

## Gdyby trzeba było wrócić

Jedna linia w `core/AppModel.kt`: `FOREGROUND_POLL_MS = 2_000L` → `8_000L`.
Nic więcej nie zależy od tej stałej.

## Paczka

- ZIP (pełny projekt, 144 pliki): **1 166 660 B**, SHA-256
  `e8a69808bf9d7d1cdebabc07a73d68d4b49fa5c381e07ecc1457d05858b8eed1`
- Dysk: `1B5G35wnp5m024mWwIq-puqscb7jcIPaE` (publiczny)
- W repo: `android-sync/v0.31.56-szybki-polling/`

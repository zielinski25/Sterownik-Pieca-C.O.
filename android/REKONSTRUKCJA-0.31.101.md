# Android 0.31.101 — odtworzone źródła (28.11.2026)

Pełny projekt odtworzony 1:1 z:

1. bazy **0.31.91** — `SterownikCO-zrodla-a6b3b674.zip` (Google Drive, 15.11.2026),
2. łańcucha 10 poprawek z repozytorium `zielinski25/ryby-led`
   (gałąź `arena/318fd089-ryby-led`, folder `dodatki/temperatury-zew/`),
   nałożonych po kolei `git apply --ignore-space-change` dokładnie tak,
   jak robiły to skrypty `buduj-*.ps1` na PC:

| krok | plik poprawki | wynik |
|---|---|---|
| 1 | `android-0.31.91-do-0.31.92.patch` | 0.31.92 |
| 2 | `android-0.31.92-do-0.31.93-wykresy.patch` | 0.31.93 |
| 3 | `android-0.31.93-do-0.31.94-bez-zew3.patch` | 0.31.94 |
| 4 | `android-0.31.94-do-0.31.95-widget-minus25.patch` | 0.31.95 |
| 5 | `android-0.31.95-do-0.31.96-pogoda.patch` | 0.31.96 |
| 6 | `android-0.31.96-do-0.31.97-kafelek-zew.patch` | 0.31.97 |
| 7 | `android-0.31.97-do-0.31.98-kompilacja.patch` | 0.31.98 |
| 8 | `android-0.31.98-do-0.31.99-pogoda-czujniki.patch` | 0.31.99 |
| 9 | `android-0.31.99-do-0.31.100-widget-zew.patch` | 0.31.100 |
| 10 | `android-0.31.100-do-0.31.101-widget-przecinek.patch` | 0.31.101 |

Wszystkie poprawki nałożyły się bez konfliktów. `app/build.gradle.kts`:
`versionCode = 31101`, `versionName = "0.31.101"` — ta sama wersja, która
jest zainstalowana na telefonie (`0.31.101-debug`).

Od teraz źródła aplikacji żyją **tylko tutaj** (`android/` w repo
`Sterownik-Pieca-C.O.`). Każda kolejna wersja = pełny projekt w repo + ZIP
całego projektu na Google Drive, nigdy sama łatka.

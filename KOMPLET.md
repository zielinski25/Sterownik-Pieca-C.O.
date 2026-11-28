# SterownikCO — KOMPLET (stan na 28.11.2026)

Jedno miejsce z wszystkim, co potrzebne do odtworzenia każdego elementu systemu.
Źródłem prawdy jest **to repozytorium** (`zielinski25/Sterownik-Pieca-C.O.`, gałąź
`arena/f170eab1-sterownik-pieca-c-o`, PR #22). Kopia offline:
`SterownikCO-KOMPLET-2026-11-28.zip` na Google Drive → `01-Aktualne`.

| element | wersja | gdzie w repo | gdzie w ZIP-ie KOMPLET | stan na sprzęcie |
|---|---|---|---|---|
| Aplikacja Android | **0.31.102** (`com.sterownikco.pro`) | `android/` | `android-0.31.102/` | zbudowana 28.11 na PC; 0.31.101 na telefonie → zainstaluj 0.31.102 |
| Centrala ESP32-S3 | **v3.58.23** (`main_centrala.cpp`) | `firmware-patch/v3.58.23-dht22-u2/` | `centrala-3.58.23/` | wgrana **3.58.22**; 3.58.23 dostarczona, nie wgrana |
| Centrala — poprzednia | v3.58.22 | `firmware-patch/v3.58.22-dht22-log/` | `centrala-3.58.22-WGRANA/` | **wgrana** |
| Panel LCD (ESP32 + ILI9341) | **v1.5.20** | `LCD-Centrali/` (pełny projekt PlatformIO) | `panel-lcd-1.5.20/` | wgrana v1.5.20 (test dźwięku — czeka na raport) |
| Strona WWW centrali | wbudowana w firmware; kopia robocza | `Piec.html` | `www/Piec.html` | — |
| Mapa czujników | — | `MAPA_CZUJNIKOW.md` | `dokumenty/` | — |

## Jak zbudować

**Android** — w folderze projektu: `powershell -NoProfile -ExecutionPolicy Bypass -File .\BUDUJ-ANDROID.ps1`
(APK trafia do `Downloads\SterownikCO-<wersja>-debug.apk`). Potrzebne: Android Studio (SDK 35) + jego JDK.

**Centrala** — skopiuj `src/main_centrala.cpp` do projektu PlatformIO
`Documents\PlatformIO\sterownik-co-esp32\src\` (środowisko `esp32-s3-n16r8`), Upload.
Przed wgraniem przeczytaj `CZYTAJ-NAJPIERW.txt` danej wersji.

**Panel LCD** — folder `LCD-Centrali/` jest kompletnym projektem PlatformIO:
`pio run -e esp32dev_wifi -t upload` (OTA 192.168.4.50:3232) albo `esp32dev` po kablu.
Testy hosta: `testy_host/uruchom_testy.sh`.

## Historia odtworzenia źródeł Androida
Wersje 0.31.92–0.31.101 istniały tylko jako łatki w repo `zielinski25/ryby-led`
(`dodatki/temperatury-zew/`); folder roboczy na PC skasował „SprzatanieDysku" 27.11.
Odtworzono je 28.11 (patrz `android/REKONSTRUKCJA-0.31.101.md`). **Od teraz każda
wersja aplikacji = pełny projekt w `android/` + pełny ZIP na Drive. Żadnych łatek.**

## Linki Google Drive (folder projektu `12mi44TeZFlcVOc8qw1NPBOH5nSP0mHgz`)
- `01-Aktualne/SterownikCO-KOMPLET-2026-11-28.zip` — ten komplet
- `03-Android/SterownikCO-Android-0.31.102-PELNY-PROJEKT.zip` — https://drive.google.com/file/d/1toCtj8TsqSIhhdWGdGDoX10EPc6WA0jx/view
- `03-Android/SterownikCO-Android-0.31.101-PELNY-PROJEKT.zip` — https://drive.google.com/file/d/1FJFGy05z8NHuWZuLoMSlTaio84GXphAa/view
- centrala 3.58.23 — https://drive.google.com/file/d/17O3jMP5VaUH4AiLQzUrD38VnGd9MsJjK/view
- centrala 3.58.22 (wgrana) — https://drive.google.com/file/d/1WRhl6mJILSxPY1HII7Qv5omfDtFuhjLE/view
- panel LCD 1.5.20 — https://drive.google.com/file/d/1oZ20ikweYvjiMFakWOawafZ7OQq9V16V/view

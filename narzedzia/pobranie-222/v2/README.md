# Wersja 2 paczki do wgrania (2026-10-28)

Powód: wersja 1 miała tylko 4 pliki i PlatformIO użytkownika zatrzymał się na
`src/main_centrala.cpp:5162:10: fatal error: CheckedHistoryIO.h: No such file or directory`.
Brakowało nagłówków wprowadzonych w firmware 3.55.1–3.55.8.

## Zawartość (17 wpisów)

`main_centrala.cpp` (v3.56.4, sha `f89874f3…`) + 14 nagłówków lokalnych +
`SPRAWDZ.ps1` + `CZYTAJ-TO.txt`.

Nagłówki i SHA256 (wszystkie z tego repo, bez zmian):

| plik | źródło |
|---|---|
| CheckedHistoryIO.h | firmware-patch/v3.55.8-blokada-fs-po-starcie |
| EspVfsCalls.h | j.w. |
| HistoryStartup.h | j.w. |
| HistoryBoot.h | j.w. |
| BootFilesystem.h | j.w. |
| Storage.h | j.w. |
| Transition.h | j.w. |
| AlarmBuzzerProfiles.h | firmware-patch/v3.54.0-offline-buffer-toggle |
| AutoIgnitionColdTimer.h | j.w. |
| MieszadloTrocinyAuto.h | j.w. |
| SolarDailyArchive.h | j.w. |
| HistoriaPin.h | narzedzia/wysylka-zapinka211 (etap 211) |
| HistoriaPobranie.h | narzedzia/od-przycisku216 (etap 216) |
| HistoriaBilety.h | narzedzia/panel-pobrania213 (etap 213) |

Pełne SHA256: `HASHES.json`.

## Kontrola pokrycia

`main_centrala.cpp` wciąga lokalnie 16 plików: 12 nagłówków własnych + `secrets.h`
+ 3 z bibliotek (`RTClib.h`, `dhtESP32-rmt.h`, `mbedtls/platform.h`).
Wszystkie 12 własnych jest w paczce; `secrets.h` zostaje użytkownika.

## Zgodność bibliotek

PlatformIO użytkownika ma `ESPAsyncWebServer @ 3.12.1` i `AsyncTCP @ 3.4.10`.
3.12.1 to **dokładnie** commit `c47917dcaba2acefb04277cd679f58a60b4fab30`,
na którym robione były etapy 220–222 (`AsyncURIMatcher` obecny, 58 wystąpień).

## ZIP

`PROJEKT-v3.56.4-PELNY.zip` — 723 209 B, 17 wpisów,
sha256 `0416dc88e2c7ac19525fd1b69957d10fe0b9b94fa8a1db34013229b12cc787d8`,
Drive id `1cfsnc6IdN3RYljzbMiYMwBs19BtT3iYD`.

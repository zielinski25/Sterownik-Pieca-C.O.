# Etap 222 — gotowe pliki + komendy PowerShell

Paczka na Drive: `PROJEKT-v3.56.4-DO-WGRANIA.zip` (694 959 B,
sha256 `1af31910…cf2f`), id pliku `1fgkX7gPzVpHi60d2-b3ZSV3qDCRCvRIo`.

Zawartość (pliki na wierzchu, bez podfolderu):

| plik | bajtów | sha256 |
|---|---|---|
| main_centrala.cpp | 2 376 655 | `f89874f3…8e38` |
| HistoriaPobranie.h | 11 835 | `b58d1a4c…ede8` |
| HistoriaBilety.h | 4 982 | `339f83fd…de30` |
| HistoriaPin.h | 7 190 | `54751238…f307` |
| SPRAWDZ.ps1 | — | skrypt sprawdzający |
| CZYTAJ-TO.txt | — | krótka instrukcja |

## Poprawne numery linii w v3.56.4

Wcześniej podałem 5601 i 5242–5244 — te numery pochodziły z innej wersji pliku.
W pliku, który jest w paczce, jest tak:

- `static const char* FIRMWARE_VERSION = "v3.56.4";` — **linia 5611**
- `#include "HistoriaPin.h"` — **linia 5166**
- `#include <ESPAsyncWebServer.h>` — **linia 5252**
- `#include "HistoriaPobranie.h"` — **linia 5253**
- `#include "HistoriaBilety.h"` — **linia 5254**
- trasy `exact("/history")`, `exact("/history/long")`, `exact("/history/link")`
  — linie **28772, 28775, 28783**; gołych `wifiWebServer.on("/history"` — **0**

Plik ma 46 994 linie i 2 376 655 B.

## Kolejność

1. Pobrać ZIP z Drive (przycisk pobierania).
2. Znaleźć folder projektu (komenda w czacie).
3. `Expand-Archive` do `%TEMP%\v3564`, skopiować 4 pliki do projektu.
4. Uruchomić `SPRAWDZ.ps1` — musi pokazać `SHA256 OK` przy `main_centrala.cpp`.
5. Dopisać w `secrets.h` linię `#define SECRET_HISTORIA_TOKEN "…"`.
6. Zbudować (`pio run` albo przycisk Build w VS Code). Wgrywa użytkownik.

## Czego paczka nie robi

Nie zmienia `secrets.h` (hasła użytkownika), nie wgrywa nic na urządzenie,
nie rusza NVS, ustawień, OTA ani rytmu 8 s / 60 s.

# Sterownik-Pieca-C.O.

Sterownik pieca C.O. (ESP) — repozytorium przebudowane od zera.

## Struktura

- `firmware/` — firmware ESP (main_centrala.cpp)
- `panel/` — panel lokalny (Piec.html)
- `docs/` — dokumentacja i konfiguracja
- `centrala-testowa-c3/` — **centrala-zastępnik na ESP32** (env `esp32-c3`,
  `esp32` i `esp32s3` — ten sam kod, wybór zależy tylko od tego, jaka płytka
  jest pod ręką): ten sam kontrakt
  HTTP, który czyta panel LCD (AP `PiecCO_AP`, `/api/status`,
  `/api/ustawienia-get/set`, `/api/servo-manual`, `/api/pompa-manual`,
  `/api/history`), ale dane z symulacji pieca. Do testów panelu LCD bez
  prawdziwej centrali (S3) i bez czujników. Zobacz
  `centrala-testowa-c3/README.md`.

## Archiwa źródłowe

- `Cenatrala Pieca.zip` — pełne źródło centrali (ESP32-S3 N16R8)
- `LCD Centrali.zip` — pełne źródło panelu LCD (ESP32 + TFT 3,5" + LVGL)

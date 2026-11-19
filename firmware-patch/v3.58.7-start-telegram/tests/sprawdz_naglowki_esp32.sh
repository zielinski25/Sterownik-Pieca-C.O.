#!/usr/bin/env bash
# ============================================================================
# Kod centrali 3.58.7 [v3.58.7 START-TG] sprawdzony PRAWDZIWYMI naglowkami ESP32
# (Arduino.h, Preferences.h, esp_system.h, esp_ota_ops.h) - -fsyntax-only, clang
# z pakietu Pythona "ziglang", te same pobrane naglowki co testy panelu LCD
# (LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh). Co jest wycinane -
# patrz wytnij_start_tg.py. Dodatkowo uruchamia sprawdzenie z paczki 3.58.6
# (bloki PANEL-OTA i PANEL-WYKRESY) na pliku 3.58.7.
#   ./sprawdz_naglowki_esp32.sh                rdzen arduino-esp32 3.0.7
#   RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh    rdzen 3.3.8 (ten ma centrala)
#   OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh  liczy tez ostrzezenia clang
# To NIE jest build PlatformIO calego pliku (50 tys. linii, biblioteki i .h
# projektu, ktorych nie ma w repozytorium) - uzytkownik kompiluje sam.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
PANEL="../../../LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh"
if [ ! -f "$PANEL" ]; then
  echo "Brak $PANEL - ten skrypt dziala tylko w pelnym repozytorium."
  exit 2
fi
ROB="${TMPDIR:-/tmp}/centrala_start_tg_test/naglowki"
mkdir -p "$ROB"
echo "[A] wycinam kod [v3.58.7 START-TG] z $CPP"
python3 wytnij_start_tg.py "$CPP" "$ROB/start_tg_esp32.cpp" | sed 's/^/    /'
echo "[B] sprawdzanie prawdziwymi naglowkami (skrypt panelu LCD)"
SRC="$ROB" PLIKI="start_tg_esp32.cpp" "$PANEL"
TESTY_3586="${TESTY_3586:-../../v3.58.6-panel-ota/tests}"
if [ -x "$TESTY_3586/sprawdz_naglowki_esp32.sh" ]; then
  echo "[C] bloki PANEL-OTA i PANEL-WYKRESY (skrypt z paczki 3.58.6) na pliku 3.58.7"
  CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
  ( cd "$TESTY_3586" && CPP_PATH="$CPP_ABS" ./sprawdz_naglowki_esp32.sh )
fi

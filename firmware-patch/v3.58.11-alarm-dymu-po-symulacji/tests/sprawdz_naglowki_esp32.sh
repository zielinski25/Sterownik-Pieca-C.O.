#!/usr/bin/env bash
# ============================================================================
# Kod centrali 3.58.11 sprawdzony PRAWDZIWYMI naglowkami ESP32 (clang z pakietu
# "ziglang", naglowki arduino-esp32 pobrane przez skrypt panelu LCD):
#   [A] blok DYM-SYM + fragment pozar() (Arduino.h: String, F(), noTone,
#       digitalWrite, millis) - wytnij_dym_sym_esp32.py,
#   [B] skrypt paczki 3.58.10 (Zew 2 / wilgotnosc(), START-TG, PANEL-OTA,
#       PANEL-WYKRESY) na tym pliku - ten kod sie nie zmienil.
#   ./sprawdz_naglowki_esp32.sh                rdzen arduino-esp32 3.0.7
#   RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh    rdzen 3.3.8 (ten ma centrala)
# To NIE jest build PlatformIO calego pliku - uzytkownik kompiluje sam.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
PANEL="../../../LCD-Centrali/testy_host/sprawdz_naglowki_esp32.sh"
if [ ! -f "$PANEL" ]; then
  echo "Brak $PANEL - ten skrypt dziala tylko w pelnym repozytorium."
  exit 2
fi
ROB="${TMPDIR:-/tmp}/centrala_35811_test/naglowki"
mkdir -p "$ROB"
echo "[A] wycinam blok DYM-SYM + fragment pozar() z $CPP"
python3 wytnij_dym_sym_esp32.py "$CPP_ABS" "$ROB/dym_sym_esp32.cpp" | sed 's/^/    /'
SRC="$ROB" PLIKI="dym_sym_esp32.cpp" "$PANEL"
TESTY_35810="${TESTY_35810:-../../v3.58.10-zew2-ponowienie/tests}"
if [ -x "$TESTY_35810/sprawdz_naglowki_esp32.sh" ]; then
  echo "[B] bloki Zew 2 / wilgotnosc(), START-TG, PANEL-OTA, PANEL-WYKRESY (skrypt z paczki 3.58.10) na pliku 3.58.11"
  sed 's/FIRMWARE_VERSION = "v3.58.11"/FIRMWARE_VERSION = "v3.58.10"/' "$CPP_ABS" > "$ROB/jako_35810.cpp"
  ( cd "$TESTY_35810" && CPP_PATH="$ROB/jako_35810.cpp" ./sprawdz_naglowki_esp32.sh )
fi

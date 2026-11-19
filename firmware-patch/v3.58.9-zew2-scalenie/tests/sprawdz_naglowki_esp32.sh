#!/usr/bin/env bash
# ============================================================================
# Kod centrali 3.58.9 sprawdzony PRAWDZIWYMI naglowkami ESP32 (clang z pakietu
# "ziglang", naglowki arduino-esp32 pobrane przez skrypt panelu LCD):
#   [A] blok Zew 2 + filtr [v3.58.9 ZEW2-FILTR] + cala funkcja wilgotnosc()
#       (Arduino.h: String, millis, portMUX_TYPE, esp_timer_get_time, isfinite/fabsf,
#        DLOGF z %.1f i rzutowaniem (double)) - wytnij_wilgotnosc_esp32.py,
#   [B] bloki START-TG (skrypt paczki 3.58.7) oraz PANEL-OTA i PANEL-WYKRESY
#       (skrypt paczki 3.58.6) na tym pliku - ten kod sie nie zmienil.
#   ./sprawdz_naglowki_esp32.sh                rdzen arduino-esp32 3.0.7
#   RDZEN=3.3.8 ./sprawdz_naglowki_esp32.sh    rdzen 3.3.8 (ten ma centrala)
#   OSTRZEZENIA=1 ./sprawdz_naglowki_esp32.sh  liczy tez ostrzezenia clang
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
ROB="${TMPDIR:-/tmp}/centrala_3589_test/naglowki"
mkdir -p "$ROB"
echo "[A] wycinam blok Zew 2 + filtr + wilgotnosc() z $CPP"
python3 wytnij_wilgotnosc_esp32.py "$CPP_ABS" "$ROB/wilgotnosc_esp32.cpp" | sed 's/^/    /'
echo "    oraz blok [v3.58.7 START-TG] (kopia skryptu z paczki 3.58.8, numer wersji 3.58.9)"
python3 wytnij_start_tg.py "$CPP_ABS" "$ROB/start_tg_esp32.cpp" | sed 's/^/    /'
SRC="$ROB" PLIKI="wilgotnosc_esp32.cpp start_tg_esp32.cpp" "$PANEL"
TESTY_3586="${TESTY_3586:-../../v3.58.6-panel-ota/tests}"
if [ -x "$TESTY_3586/sprawdz_naglowki_esp32.sh" ]; then
  echo "[B] bloki PANEL-OTA i PANEL-WYKRESY (skrypt z paczki 3.58.6) na pliku 3.58.9"
  ( cd "$TESTY_3586" && CPP_PATH="$CPP_ABS" ./sprawdz_naglowki_esp32.sh )
fi

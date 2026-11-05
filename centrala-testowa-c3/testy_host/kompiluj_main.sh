#!/usr/bin/env bash
# ============================================================================
# Sprawdzenie SKLADNI warstwy Arduino (src/main.cpp) na PC.
# ============================================================================
# Nie da sie tu zbudowac firmware'u dla ESP32 (brak toolchaina riscv32 w tym
# srodowisku), ale mozna podstawic minimalne atrapy Arduino/WiFi/Preferences
# (testy_host/mock/) i sprawdzic, czy main.cpp + strona_www.h kompiluja sie
# bez bledow skladni i typow. To wylapuje literowki w ~900 liniach.
#
#   ./kompiluj_main.sh
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
echo "[1/2] skladnia main.cpp (atrapy Arduino):"
g++ -std=c++17 -fsyntax-only -Wall -Wextra -Wno-unused-parameter \
    -I mock -I ../src ../src/main.cpp
echo "      OK"
echo "[2/2] test rdzenia + weryfikacja kontraktu API:"
./uruchom_testy.sh

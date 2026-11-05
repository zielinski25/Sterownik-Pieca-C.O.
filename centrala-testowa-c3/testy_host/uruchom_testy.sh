#!/usr/bin/env bash
# ============================================================================
# Test rdzenia centrali testowej NA PC (bez plytki ESP32-C3).
# Kompiluje przenosna czesc projektu zwyklym g++, rozkreca 3 h symulacji
# i sprawdza, czy JSON-y/CSV zgadzaja sie z tym, co czyta panel LCD.
#
#   ./uruchom_testy.sh
#
# Wymaga: g++ (C++17), python3.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"

BIN="${TMPDIR:-/tmp}/centrala_c3_test_core"

echo "[1/3] kompilacja rdzenia (g++, -Wall -Wextra)..."
g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
    -o "$BIN" test_core.cpp ../src/symulacja.cpp

echo "[2/3] uruchomienie symulacji (3 h wirtualnego czasu)..."
"$BIN" > "${TMPDIR:-/tmp}/centrala_c3_wynik.txt"

echo "[3/3] weryfikacja kontraktu API..."
python3 sprawdz_wynik.py < "${TMPDIR:-/tmp}/centrala_c3_wynik.txt"

#!/usr/bin/env bash
# Uruchamia wszystkie symulacje i testy 3.57.6 (albo najnowszej wersji w repo).
# Uzycie:  bash narzedzia/symulacje-firmware/run-all.sh
set -uo pipefail
cd "$(dirname "$0")"
REPO="$(cd ../.. && pwd)"

FW_DIR="$(ls -d "$REPO"/firmware-patch/v3.57.*-hist-fs-opcja 2>/dev/null | sort -V | tail -1)"
if [ -z "${FW_DIR:-}" ]; then echo "Brak folderu firmware-patch/v3.57.*-hist-fs-opcja"; exit 2; fi
FW_SRC="$FW_DIR/src"
echo "Firmware: $(basename "$FW_DIR")"

WYNIK=0

echo
echo "════════ 1/3  Test statyczny (firmware + oba panele) ════════"
node "$REPO/narzedzia/testy-firmware/test-hist-fs.js" || WYNIK=1

echo
echo "════════ 2/3  Symulacja A1 i bramek zapisu (C++) ════════"
if ! command -v g++ >/dev/null 2>&1; then
  echo "BRAK g++ — pomijam symulacje C++"
  WYNIK=1
else
  g++ -std=gnu++17 -O1 -Wall -Wextra -I "$FW_SRC" -o /tmp/a1_sim_$$ a1_sim.cpp || { echo "Kompilacja nieudana"; exit 1; }
  /tmp/a1_sim_$$ || WYNIK=1
  rm -f /tmp/a1_sim_$$
fi

echo
echo "════════ 3/3  Symulacja panelu w DOM (jsdom) ════════"
if [ ! -d /tmp/domjs/node_modules/jsdom ]; then
  echo "Instaluje jsdom do /tmp/domjs ..."
  mkdir -p /tmp/domjs
  (cd /tmp/domjs && npm i --silent jsdom@24 >/dev/null 2>&1)
fi
if [ -d /tmp/domjs/node_modules/jsdom ]; then
  NODE_PATH=/tmp/domjs/node_modules node panel_sim.js || WYNIK=1
else
  echo "BRAK jsdom (brak sieci?) — pomijam symulacje panelu"
  WYNIK=1
fi

echo
if [ "$WYNIK" -eq 0 ]; then echo "WSZYSTKO OK"; else echo "SA BLEDY — patrz wyzej"; fi
exit "$WYNIK"

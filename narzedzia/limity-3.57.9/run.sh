#!/usr/bin/env bash
# Uruchamia cala symulacje limitow i test zgodnosci ze zrodlem firmware.
#
#   bash narzedzia/limity-3.57.9/run.sh
#   FIRMWARE_SRC=/sciezka/src/main_centrala.cpp bash narzedzia/limity-3.57.9/run.sh
#
# Bez FIRMWARE_SRC test zgodnosci konczy sie komunikatem "POMINIETO"
# (zrodlo firmware nie jest trzymane w repo - lezy w paczce ZIP na Dysku).
set -u
cd "$(dirname "$0")" || exit 1

BIN="${TMPDIR:-/tmp}/limity-3.57.9"
rc=0

echo "=== 1/2  symulacja C++ (prawdziwy history193::Engine) ==="
if ! g++ -std=gnu++17 -Wall -Wextra -O2 -o "$BIN" test_limity.cpp; then
  echo "Kompilacja nieudana."
  exit 1
fi
"$BIN" || rc=1

echo
echo "=== 2/2  zgodnosc z zrodlem firmware ==="
if [ -n "${FIRMWARE_SRC:-}" ]; then
  python3 test_zgodnosc.py --src "$FIRMWARE_SRC" || rc=1
else
  python3 test_zgodnosc.py
fi

echo
if [ "$rc" -eq 0 ]; then
  echo "WYNIK: wszystko zielone."
else
  echo "WYNIK: sa niepowodzenia (patrz FAIL powyzej)."
fi
exit "$rc"

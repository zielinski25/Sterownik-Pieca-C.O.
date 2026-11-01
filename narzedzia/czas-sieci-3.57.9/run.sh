#!/usr/bin/env bash
# Model czasu sieci 3.57.8 + odczyt parametrow ze zrodla firmware.
#
#   FIRMWARE_SRC=/sciezka/src/main_centrala.cpp bash narzedzia/czas-sieci-3.57.9/run.sh
#   bash narzedzia/czas-sieci-3.57.9/run.sh      # wartosci domyslne = to, co jest w 3.57.8
set -u
cd "$(dirname "$0")" || exit 1
rc=0

if [ -n "${FIRMWARE_SRC:-}" ]; then
  echo "=== 1/2  parametry odczytane z firmware ==="
  python3 zbierz_parametry.py --src "$FIRMWARE_SRC" || rc=1
  echo
  echo "=== 2/2  model czasu + asercje ==="
  python3 model_czasu.py --src "$FIRMWARE_SRC" || rc=1
else
  echo "UWAGA: brak FIRMWARE_SRC - licze na wartosciach domyslnych (3.57.8)."
  echo
  python3 model_czasu.py || rc=1
fi

echo
if [ "$rc" -eq 0 ]; then echo "WYNIK: wszystko zielone."; else echo "WYNIK: sa niepowodzenia."; fi
exit "$rc"

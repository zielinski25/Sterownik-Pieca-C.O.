#!/usr/bin/env bash
# Model czasu sieci 3.57.8 + audyt wszystkich timeoutow + brama kroku 3.
#
#   bash narzedzia/czas-sieci-3.57.9/run.sh
#   FIRMWARE_SRC=/inna/kopia/main_centrala.cpp bash .../run.sh
set -u
cd "$(dirname "$0")" || exit 1

SRC="${FIRMWARE_SRC:-../../firmware-patch/v3.57.8-raport-i-wersja/src/main_centrala.cpp}"
rc=0

echo "=== 1/4  parametry odczytane z firmware ==="
python3 zbierz_parametry.py --src "$SRC" || rc=1
echo
echo "=== 2/4  audyt wszystkich setTimeout / setHandshakeTimeout ==="
python3 audyt_timeoutow.py --src "$SRC" || rc=1
echo
echo "=== 3/4  model czasu + asercje ==="
python3 model_czasu.py --src "$SRC" || rc=1
echo
echo "=== 4/4  brama kroku 3 (po zmianie ma byc 4/4) ==="
if python3 sprawdz_po.py --src "$SRC" --baza "$SRC" >/tmp/brama.txt 2>&1; then
  cat /tmp/brama.txt
  echo "brama: wszystko zrobione."
else
  tail -4 /tmp/brama.txt
  echo "brama: FAIL — oczekiwane PRZED zmiana w kodzie (lista do zrobienia)."
fi

echo
if [ "$rc" -eq 0 ]; then echo "WYNIK: model i audyt zielone."; else echo "WYNIK: sa niepowodzenia."; fi
exit "$rc"

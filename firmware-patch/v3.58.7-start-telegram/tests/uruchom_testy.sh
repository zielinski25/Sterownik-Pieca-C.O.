#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.7 - opis: README.txt.
#   czesc C: blok [v3.58.7 START-TG] - wiadomosc startowa na Telegram i rodzaj
#            niezgodnosci TAGCHECK (nowe w tej wersji; test_start_telegram.cpp),
#   czesc A i B: testy z paczki 3.58.6 (posrednictwo OTA panelu, wykresy dla
#            panelu) uruchomione na pliku 3.58.7 - ten kod nie zmienil sie,
#            testy pilnuja, ze nadal jest taki sam.
#   ./uruchom_testy.sh              zwykle uruchomienie
#   SANITIZER=1 ./uruchom_testy.sh  to samo z AddressSanitizer + UBSan
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3. Linux albo WSL.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
ROB="${TMPDIR:-/tmp}/centrala_start_tg_test"
TESTY_3586="${TESTY_3586:-../../v3.58.6-panel-ota/tests}"
mkdir -p "$ROB"
SAN=""
if [ "${SANITIZER:-0}" = "1" ]; then SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"; fi

echo "=== C. [v3.58.7 START-TG] wiadomosc startowa + TAGCHECK ==="
echo "[C1] wycinam blok z $CPP"
python3 - "$CPP" "$ROB/start_tg_wycinek.inc" <<'EOF'
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
a = t.index("// [v3.58.7 START-TG-BEGIN]")
b = t.index("// [v3.58.7 START-TG-END]")
blok = t[a:b]
open(dst, "w", encoding="utf-8").write("#include <cstdio>\n#include <cstdint>\n#include <cstddef>\n" + blok)
print("    %d linii" % blok.count("\n"))
EOF
echo "[C2] kompilacja (g++ -Wall -Wextra -Wshadow -Wconversion -Wformat=2)"
# shellcheck disable=SC2086
g++ -std=gnu++17 -O1 -g -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Wformat=2 -Werror $SAN -I "$ROB" -o "$ROB/test_start_telegram" test_start_telegram.cpp
echo "[C3] testy"
"$ROB/test_start_telegram"

if [ -d "$TESTY_3586" ]; then
  echo
  echo "=== A + B. testy z paczki 3.58.6 na pliku 3.58.7 (kod OTA panelu i wykresow bez zmian) ==="
  CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
  # KOD_PANELU wskazuje na nieistniejacy katalog: porownanie bloku OTA z kopia w
  # testach panelu robimy nizej sami, bo wycinek ma w 1. linii numer wersji
  # pliku (3.58.7 zamiast 3.58.6) - reszta musi byc identyczna.
  ( cd "$TESTY_3586" && CPP_PATH="$CPP_ABS" KOD_PANELU="/nonexistent-kod-panelu" SANITIZER="${SANITIZER:-0}" ./uruchom_testy.sh )
  KOD_PANELU="${KOD_PANELU:-../../../LCD-Centrali/testy_host/kod_centrali}"
  if [ -f "$KOD_PANELU/panel_ota_blok.inc" ]; then
    echo "[A4'] blok decyzji OTA panelu a kopia w testach panelu LCD (bez 1. linii z numerem wersji)"
    python3 "$TESTY_3586/wytnij_blok.py" "$CPP_ABS" --ota-blok "$ROB/panel_ota_blok.inc" >/dev/null
    if cmp -s <(tail -n +2 "$ROB/panel_ota_blok.inc") <(tail -n +2 "$KOD_PANELU/panel_ota_blok.inc"); then
      echo "    zgodny: kod OTA panelu w 3.58.7 jest ten sam co w 3.58.6 (testy panelu LCD rozmawiaja z aktualnym kodem)"
    else
      echo "    ROZNICA: blok [v3.58.6 PANEL-OTA] w tym pliku rozni sie od $KOD_PANELU/panel_ota_blok.inc"
      exit 1
    fi
  fi
else
  echo "    (katalogu $TESTY_3586 nie ma - paczka rozpakowana osobno; testy A i B pominiete)"
fi

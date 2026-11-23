#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.12 - opis: README.txt.
#   A. SUWAKI-SYM: funkcja symulacjaSekcjaHtml() wycieta z HTML wbudowanego
#      w plik centrali (WIFI_LOCAL_PANEL_HTML) uruchomiona w jsdom; scenariusz
#      z telefonu (suwak bez fokusu + odswiezanie arkusza co 1 s). 13 sprawdzen.
#      Ten sam test na Piec.html z repozytorium i - kontrolnie - na 3.58.11,
#      ktore MA pokazac blad.
#   B. plik 3.58.12 = 3.58.11 + patch (ta sama suma SHA); skladnia JS obu
#      blokow <script> (node --check); poza HTML i komentarzami nic sie nie
#      zmienilo (patch ma 7 hunkow: naglowek, historia, FIRMWARE_VERSION,
#      4 w symulacjaSekcjaHtml()).
#   C. testy paczki 3.58.11 czesc A (DYM-SYM, 21 sprawdzen) na tym pliku.
#   ./uruchom_testy.sh
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: node >= 18 + pakiet jsdom (npm i jsdom w katalogu JSDOM_DIR, domyslnie
# /tmp/jsd), python3, patch, sha256sum. Czesci B-C wymagaja pelnego repozytorium.
# ============================================================================
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
JSDOM_DIR="${JSDOM_DIR:-/tmp/jsd}"
if [ ! -d "$JSDOM_DIR/node_modules/jsdom" ]; then
  echo "instaluje jsdom w $JSDOM_DIR"; mkdir -p "$JSDOM_DIR"; (cd "$JSDOM_DIR" && npm init -y >/dev/null && npm i jsdom@24 --silent)
fi
export NODE_PATH="$JSDOM_DIR/node_modules"
BLEDY=0
echo "=== A. SUWAKI-SYM: suwaki symulacji nie cofaja sie pod palcem (symulacjaSekcjaHtml() z $CPP) ==="
node test_suwaki_symulacji.js "$CPP_ABS" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ -f ../../../Piec.html ]; then
  echo "[A2] to samo na Piec.html z repozytorium"
  node test_suwaki_symulacji.js ../../../Piec.html | sed 's/^/    /' || BLEDY=$((BLEDY+1))
fi
if [ -f ../../v3.58.11-alarm-dymu-po-symulacji/src/main_centrala.cpp ]; then
  echo "[A3] kontrolnie 3.58.11 - ma pokazac blad (suwak wraca do 21.0)"
  W11=$(node test_suwaki_symulacji.js ../../v3.58.11-alarm-dymu-po-symulacji/src/main_centrala.cpp || true)
  if echo "$W11" | grep -q "NIEZALICZONY"; then echo "    [OK] 3.58.11 nie zalicza: $(echo "$W11" | grep -c BLAD) z 13 sprawdzen BLAD (blad odtworzony)"; else echo "    BLAD: 3.58.11 zalicza?"; BLEDY=$((BLEDY+1)); fi
fi

echo "=== B. plik 3.58.12 = 3.58.11 + patch; skladnia JS ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.12";') == 1, 'FIRMWARE_VERSION = "v3.58.12"')
spr(s.count('const symEdycja = { wartosc: false, czas: false };') == 1, 'symEdycja zadeklarowane raz (jedna funkcja symulacjaSekcjaHtml)')
spr(s.count('[v3.58.12') >= 6, 'znaczniki [v3.58.12 ...] (%d)' % s.count('[v3.58.12'))
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
for i, b in enumerate(re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.11-alarm-dymu-po-symulacji/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35812_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.11-alarm-dymu-po-symulacji/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p0 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.11-do-v3.58.12.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.11 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  H=$(grep -c '^@@' ../main_centrala-v3.58.11-do-v3.58.12.patch); echo "    patch: $H hunkow, +$(grep -c '^+[^+]' ../main_centrala-v3.58.11-do-v3.58.12.patch) / -$(grep -c '^-[^-]' ../main_centrala-v3.58.11-do-v3.58.12.patch)"
fi

echo "=== C. testy paczki 3.58.11 czesc A (DYM-SYM) na tym pliku ==="
T11=../../v3.58.11-alarm-dymu-po-symulacji/tests
if [ -x "$T11/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.12"/FIRMWARE_VERSION = "v3.58.11"/' "$CPP_ABS" > /tmp/jako_35811.cpp
  # czesc B tamtego skryptu porownuje SHA z 3.58.11 - tu z definicji inna; liczy sie czesc A
  (CPP_PATH=/tmp/jako_35811.cpp "$T11/uruchom_testy.sh" 2>&1 || true) | sed -n '1,/^=== B/p' | grep -v '^=== B' | sed 's/^/    /'
  if (CPP_PATH=/tmp/jako_35811.cpp "$T11/uruchom_testy.sh" 2>&1 || true) | grep -q "RAZEM: 21 sprawdzen OK, 0 BLAD"; then echo "    [OK] DYM-SYM 21/21"; else echo "    BLAD: DYM-SYM"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

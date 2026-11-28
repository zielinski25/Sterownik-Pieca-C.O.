#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.14 (ZEWN-ZRODLO) - opis: README.txt.
#   A. JS: applyIncomingData() + sekcja "Zrodlo temperatury zewnetrznej" wyciete
#      z HTML wbudowanego w plik centrali, uruchomione w jsdom; 18 sprawdzen
#      (wspolzaleznosc: wybor przychodzi statusem z centrali, klikniecie wysyla
#      "ustaw zewnZrodlo N" bez stanu optymistycznego, Zew 2 bez odczytu = BRAK
#      DANYCH, symulacja ma pierwszenstwo, stare firmware, smieci). To samo
#      na Piec.html; kontrolnie 3.58.13 nie ma sekcji (test konczy sie kodem 2).
#   B. C++: galaz ustawParametrPieca("zewnZrodlo"), odczyt NVS w setup() i trzy
#      linie JSON wyciete z pliku, g++ -Wall -Wextra -Werror; 12 sprawdzen.
#   C. plik 3.58.14 = 3.58.13 + patch (SHA); FIRMWARE_VERSION; node --check JS;
#      komenda "ustaw" bez bialej listy (generyczna); klucz NVS <= 15 znakow.
#   D. testy paczek 3.58.13 (A-B: SUWAK-PO-ACK, SYM-OD-RAZU) i 3.58.11 (DYM-SYM)
#      na tym pliku.
#   ./uruchom_testy.sh
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: node >= 18 + jsdom (npm i jsdom@24 w JSDOM_DIR, domyslnie /tmp/jsd),
# g++, python3, patch, sha256sum. Czesci C-D wymagaja pelnego repozytorium.
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
echo "=== A. ZEWN-ZRODLO (JS): applyIncomingData() + zewnZrodloSekcjaHtml() z $CPP ==="
node test_zrodlo_zew.js "$CPP_ABS" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ -f ../../../Piec.html ]; then
  echo "[A2] to samo na Piec.html z repozytorium"
  node test_zrodlo_zew.js ../../../Piec.html | sed 's/^/    /' || BLEDY=$((BLEDY+1))
fi
if [ -f ../../v3.58.13-symulacja-od-razu/src/main_centrala.cpp ]; then
  echo "[A3] kontrolnie 3.58.13 - nie ma sekcji wyboru zrodla (test ma zglosic brak)"
  if node test_zrodlo_zew.js ../../v3.58.13-symulacja-od-razu/src/main_centrala.cpp >/dev/null 2>&1; then echo "    BLAD: 3.58.13 zalicza?"; BLEDY=$((BLEDY+1)); else echo "    [OK] 3.58.13 nie zalicza (brak ZEWN_ZRODLA)"; fi
fi

echo "=== B. ZEWN-ZRODLO (C++): galaz ustawParametrPieca, NVS, JSON - g++ -Wall -Wextra -Werror ==="
python3 wytnij_zrodlo_zew.py "$CPP_ABS" > /tmp/zrodlo_zew.inc
g++ -std=c++17 -Wall -Wextra -Werror test_zrodlo_zew.cpp -o /tmp/t_zrodlo_zew && /tmp/t_zrodlo_zew | sed 's/^/    /' || BLEDY=$((BLEDY+1))

echo "=== C. plik 3.58.14 = 3.58.13 + patch; wersja; skladnia JS; spojnosc ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.14";') == 1, 'FIRMWARE_VERSION = "v3.58.14"')
spr(s.count('byte zewnZrodlo = 1;') == 1, 'zmienna zewnZrodlo zadeklarowana raz, domyslnie 1')
spr(s.index('byte zewnZrodlo = 1;') < s.index('} else if (pole == "zewnZrodlo") {'), 'deklaracja przed ustawParametrPieca()')
spr(s.count('config.putUChar("zewnZrodlo"') == 1 and s.count('config.getUChar("zewnZrodlo"') == 1, 'NVS: jeden zapis, jeden odczyt')
spr(s.count('"zewnZrodlo\\":') == 3, 'pole "zewnZrodlo" w 3 JSON-ach (status pelny, /api/status, /api/ustawienia-get)')
spr('if (cmd.startsWith("ustaw ")) {' in s.replace('else ', '') and 'ustawParametrPieca(poleUst, wUst, errUst)' in s, 'komenda "ustaw <pole> <w>" generyczna (bez bialej listy) -> dziala dla zewnZrodlo')
spr(s.count('[v3.58.14') >= 10, 'znaczniki [v3.58.14 ...] (%d)' % s.count('[v3.58.14'))
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
spr(html.count("sendCommand('ustaw zewnZrodlo ' + v)") == 1, "JS: przycisk wysyla 'ustaw zewnZrodlo N'")
spr(html.count('zewnZrodlo: [1, 2]') == 1 and "'zewnZrodlo'   // [v3.58.14" in html, 'JS: numericRules + integerKeys dla zewnZrodlo')
spr(html.count('t_zewn_2: [-40, 80]') == 1, 'JS: numericRules dla t_zewn_2')
for i, b in enumerate(re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.13-symulacja-od-razu/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35814_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.13-symulacja-od-razu/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.13-do-v3.58.14.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.13 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  H=$(grep -c '^@@' ../main_centrala-v3.58.13-do-v3.58.14.patch); echo "    patch: $H hunkow, +$(grep -c '^+[^+]' ../main_centrala-v3.58.13-do-v3.58.14.patch) / -$(grep -c '^-[^-]' ../main_centrala-v3.58.13-do-v3.58.14.patch)"
fi

echo "=== D. testy paczek 3.58.13 (A-B) i 3.58.11 (DYM-SYM) na tym pliku ==="
T13=../../v3.58.13-symulacja-od-razu/tests
if [ -x "$T13/uruchom_testy.sh" ]; then
  if (cd "$T13" && node test_suwaki_symulacji.js "$CPP_ABS" 2>&1 || true) | grep -q "ZALICZONY"; then echo "    [OK] SUWAK-PO-ACK 18/18"; else echo "    BLAD: SUWAK-PO-ACK"; BLEDY=$((BLEDY+1)); fi
  if (cd "$T13" && python3 wytnij_sym_od_razu.py "$CPP_ABS" > /tmp/sym_od_razu.inc && g++ -std=c++17 -Wall -Wextra -Werror test_sym_od_razu.cpp -o /tmp/t_sym_od_razu && /tmp/t_sym_od_razu 2>&1 || true) | grep -q "ZALICZONY"; then echo "    [OK] SYM-OD-RAZU 11/11"; else echo "    BLAD: SYM-OD-RAZU"; BLEDY=$((BLEDY+1)); fi
fi
T11=../../v3.58.11-alarm-dymu-po-symulacji/tests
if [ -x "$T11/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.14"/FIRMWARE_VERSION = "v3.58.11"/' "$CPP_ABS" > /tmp/jako_35811.cpp
  if (CPP_PATH=/tmp/jako_35811.cpp "$T11/uruchom_testy.sh" 2>&1 || true) | grep -q "RAZEM: 21 sprawdzen OK, 0 BLAD"; then echo "    [OK] DYM-SYM 21/21"; else echo "    BLAD: DYM-SYM"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

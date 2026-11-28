#!/usr/bin/env bash
# ============================================================================
# Testy na PC firmware'u centrali 3.58.13 - opis: README.txt.
#   A. SUWAK-PO-ACK (JS): symulacjaSekcjaHtml() wycieta z HTML wbudowanego w
#      plik centrali uruchomiona w jsdom; 18 sprawdzen (13 z 3.58.12 + 5 nowych:
#      po Zastosuj status niesie stary odczyt -> suwak zostaje; odczyt od razu
#      -> odliczanie nie zamarza). To samo na Piec.html; kontrolnie 3.58.12 ma
#      NIE zaliczyc (1 BLAD - wada odtworzona).
#   B. SYM-OD-RAZU (C++): symulacjaUstawOdczytOdRazu() wycieta z pliku,
#      skompilowana g++ -Wall -Wextra -Werror z atrapami zmiennych; 11 sprawdzen.
#   C. plik 3.58.13 = 3.58.12 + patch (SHA); FIRMWARE_VERSION; oba wywolania
#      helpera (komenda "symuluj" + /api/symulacja-czujnik); node --check JS.
#   D. testy DYM-SYM z paczki 3.58.11 (21 sprawdzen) na tym pliku.
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
echo "=== A. SUWAK-PO-ACK: suwaki symulacji po Zastosuj (symulacjaSekcjaHtml() z $CPP) ==="
node test_suwaki_symulacji.js "$CPP_ABS" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ -f ../../../Piec.html ]; then
  echo "[A2] to samo na Piec.html z repozytorium"
  node test_suwaki_symulacji.js ../../../Piec.html | sed 's/^/    /' || BLEDY=$((BLEDY+1))
fi
if [ -f ../../v3.58.12-suwaki-symulacji/src/main_centrala.cpp ]; then
  echo "[A3] kontrolnie 3.58.12 - ma pokazac blad (suwak wraca po Zastosuj)"
  W12=$(node test_suwaki_symulacji.js ../../v3.58.12-suwaki-symulacji/src/main_centrala.cpp || true)
  if echo "$W12" | grep -q "NIEZALICZONY"; then echo "    [OK] 3.58.12 nie zalicza: $(echo "$W12" | grep -c BLAD) z 18 sprawdzen BLAD (blad odtworzony)"; else echo "    BLAD: 3.58.12 zalicza?"; BLEDY=$((BLEDY+1)); fi
fi

echo "=== B. SYM-OD-RAZU: symulacjaUstawOdczytOdRazu() z pliku, g++ -Wall -Wextra -Werror ==="
python3 wytnij_sym_od_razu.py "$CPP_ABS" > /tmp/sym_od_razu.inc
g++ -std=c++17 -Wall -Wextra -Werror test_sym_od_razu.cpp -o /tmp/t_sym_od_razu && /tmp/t_sym_od_razu | sed 's/^/    /' || BLEDY=$((BLEDY+1))

echo "=== C. plik 3.58.13 = 3.58.12 + patch; wywolania helpera; skladnia JS ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.13";') == 1, 'FIRMWARE_VERSION = "v3.58.13"')
spr(s.count('static void symulacjaUstawOdczytOdRazu(') == 1, 'helper zdefiniowany raz')
spr(s.count('symulacjaUstawOdczytOdRazu(pole, wartosc);') == 1, 'wywolanie w /api/symulacja-czujnik (stara strona)')
spr(s.count('symulacjaUstawOdczytOdRazu(poleSym, wartoscSym);') == 1, 'wywolanie w komendzie "symuluj" (Firebase/LAN)')
d = s.index('static void symulacjaUstawOdczytOdRazu(')
spr(d < s.index('symulacjaUstawOdczytOdRazu(pole, wartosc);') and d < s.index('symulacjaUstawOdczytOdRazu(poleSym, wartoscSym);'), 'definicja helpera przed oboma wywolaniami')
for z in ['float temperaturaOgrzewania = ', 'float temperaturaBojlera = ', 'float temperaturaNaZewnatrz = ', 'float cisnienieOdczytane = ', 'float wilgotnoscOdczytana = ']:
    spr(0 <= s.index(z) < d, 'zmienna %s zadeklarowana przed helperem' % z.split()[1])
spr(s.count('const symZadana = { wartosc: NaN, przedtem: NaN, doMs: 0 };') == 1, 'symZadana zadeklarowane raz')
spr(s.count('[v3.58.13') >= 8, 'znaczniki [v3.58.13 ...] (%d)' % s.count('[v3.58.13'))
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
for i, b in enumerate(re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.12-suwaki-symulacji/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35813_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.12-suwaki-symulacji/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.12-do-v3.58.13.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.12 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  H=$(grep -c '^@@' ../main_centrala-v3.58.12-do-v3.58.13.patch); echo "    patch: $H hunkow, +$(grep -c '^+[^+]' ../main_centrala-v3.58.12-do-v3.58.13.patch) / -$(grep -c '^-[^-]' ../main_centrala-v3.58.12-do-v3.58.13.patch)"
fi

echo "=== D. testy paczki 3.58.11 czesc A (DYM-SYM) na tym pliku ==="
T11=../../v3.58.11-alarm-dymu-po-symulacji/tests
if [ -x "$T11/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.13"/FIRMWARE_VERSION = "v3.58.11"/' "$CPP_ABS" > /tmp/jako_35811.cpp
  if (CPP_PATH=/tmp/jako_35811.cpp "$T11/uruchom_testy.sh" 2>&1 || true) | grep -q "RAZEM: 21 sprawdzen OK, 0 BLAD"; then echo "    [OK] DYM-SYM 21/21"; else echo "    BLAD: DYM-SYM"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.15 (BEZ-HISTORIA-CSV).
#   A. w HTML strony nie ma juz histBtn/histPanel/histShort/histLong ani tekstow
#      "Pobierz histori", "Krótka (24 h)", "Długa (archiwum)", "HISTORIA CSV";
#      trasy /history/link, /history, /history/long, /api/history* w C++ ZOSTAJA;
#      liczba blokow <script> spadla o 1; node --check kazdego bloku; wersja.
#   B. plik 3.58.15 = 3.58.14 + patch (SHA).
#   C. pelne testy paczki 3.58.14 (A-D: ZEWN-ZRODLO 18+12, 3.58.13, DYM-SYM) na tym pliku.
#   ./uruchom_testy.sh        CPP_PATH=... inny plik
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
BLEDY=0
echo "=== A. BEZ-HISTORIA-CSV: przycisk usuniety z HTML, trasy w C++ zostaly ==="
python3 - "$CPP_ABS" ../../v3.58.14-zrodlo-zew/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile, os
s = open(sys.argv[1], encoding='utf-8').read()
s14 = open(sys.argv[2], encoding='utf-8').read() if os.path.exists(sys.argv[2]) else None
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
for t in ['id="histBtn"', 'id="histPanel"', 'id="histShort"', 'id="histLong"', 'id="histInfo"', 'Pobierz histori', 'Krótka (24 h)', 'Długa (archiwum)', 'HISTORIA CSV', "fetch('/history/link"]:
    spr(t not in html, 'HTML bez "%s"' % t)
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.15";') == 1, 'FIRMWARE_VERSION = "v3.58.15"')
for t in ['"/history/link"', '"/history/long"', '"/api/history"', '"/api/history/long"']:
    spr(t in s, 'trasa %s w C++ nadal zarejestrowana' % t)
bl = re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)
if s14:
    h14 = s14[s14.index('R"PIECHTML(') + 11:s14.index(')PIECHTML";')]
    b14 = re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', h14, re.S)
    spr(len(bl) == len(b14) - 1, 'blokow <script>: %d (w 3.58.14: %d) - ubyl dokladnie 1' % (len(bl), len(b14)))
    spr(2500 < len(h14) - len(html) < 3600, 'HTML krotszy o %d znakow (tylko przycisk + skrypt)' % (len(h14) - len(html)))
    poza = s14[:s14.index('R"PIECHTML(')].count('\n')
    spr(s.split(')PIECHTML";')[1] == s14.split(')PIECHTML";')[1], 'C++ za HTML-em identyczne jak w 3.58.14')
for i, b in enumerate(bl):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
echo "=== B. plik 3.58.15 = 3.58.14 + patch ==="
if [ -f ../../v3.58.14-zrodlo-zew/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35815_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.14-zrodlo-zew/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.14-do-v3.58.15.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.14 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
fi
echo "=== C. testy paczki 3.58.14 (A-D) na tym pliku ==="
T14=../../v3.58.14-zrodlo-zew/tests
if [ -x "$T14/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.15"/FIRMWARE_VERSION = "v3.58.14"/' "$CPP_ABS" > /tmp/jako_35814.cpp
  W=$(CPP_PATH=/tmp/jako_35814.cpp "$T14/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "ZALICZONY|DYM-SYM|SUWAK|SYM-OD-RAZU|WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # jedyny dopuszczalny BLAD: "3.58.13 + patch = ten plik" (to inny plik - porownanie SHA z paczki 3.58.14 nie dotyczy 3.58.15)
  NB=$(echo "$W" | grep -c "BLAD" || true); NSHA=$(echo "$W" | grep -c "BLAD: SHA po patchu" || true)
  if [ "$NB" = "$NSHA" ] && [ "$NSHA" -le 1 ]; then echo "    [OK] testy 3.58.14 zaliczone (pominieto tylko porownanie SHA z 3.58.13 + patch - nie dotyczy tego pliku)"; else echo "    BLAD: testy 3.58.14"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.16 (POGODA-ZEWN).
#   A. parsowanie Open-Meteo: prawdziwy kod pobierzPrognozeDeszczu() skompilowany g++
#      (test_pogoda_parsowanie.py, 5 odpowiedzi) - pogoda_kod / pogoda_kod_opad /
#      pogoda_dzien / wiatr_porywy + deszcz_stan / deszcz_min jak dotad;
#      /api/status ma 4 nowe pola; DESZCZ_URL prosi o weather_code, is_day,
#      wind_gusts_10m; wersja; HTML/JS strony WWW bez zmian (node --check).
#   B. plik 3.58.16 = 3.58.15 + patch (SHA).
#   C. pelne testy paczki 3.58.15 (A-C, w tym 3.58.14 A-D) na tym pliku.
#   ./uruchom_testy.sh        CPP_PATH=... inny plik
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
BLEDY=0
echo "=== A. POGODA-ZEWN: parsowanie Open-Meteo, pola /api/status, URL, wersja, WWW ==="
python3 test_pogoda_parsowanie.py "$CPP_ABS" || BLEDY=$((BLEDY+1))
python3 - "$CPP_ABS" ../../v3.58.15-bez-historia-csv/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile, os
s = open(sys.argv[1], encoding='utf-8').read()
s15 = open(sys.argv[2], encoding='utf-8').read() if os.path.exists(sys.argv[2]) else None
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.16";') == 1, 'FIRMWARE_VERSION = "v3.58.16"')
st = s[s.index('String wifiZbudujJsonStatus() {'):]
st = st[:st.index('\n}\n')]
for p in ['pogoda_kod', 'pogoda_kod_opad', 'pogoda_dzien', 'wiatr_porywy']:
    spr(('\\"%s\\":' % p) in st, '/api/status ma pole "%s"' % p)
i = st.index('deszcz_min'); j = st.index('pogoda_kod')
spr(0 < j - i < 600 and 'if' not in st[i:j], 'pola pogody zaraz za deszcz_min (bez warunku - zawsze obecne)')
url = s[s.index('static const char* DESZCZ_URL ='):]
url = url[:url.index(';')]
for p in ['current=precipitation,weather_code,is_day,wind_gusts_10m', 'minutely_15=precipitation,weather_code']:
    spr(p in url, 'DESZCZ_URL: %s' % p)
spr('#define DESZCZ_LAT "51.066389"' in s and '#define DESZCZ_LON "21.509167"' in s and 'timezone=Europe%2FWarsaw' in url and 'forecast_days=2' in url, 'DESZCZ_URL: wspolrzedne, strefa i 2 dni bez zmian')
for d in ['int pogodaKod = -1;', 'int pogodaKodOpad = -1;', 'int pogodaDzien = -1;', 'float wiatrPorywy = -1.0f;']:
    spr(s.count(d) == 1, 'definicja: %s' % d)
spr('extern int pogodaKod;' in s and 'extern float wiatrPorywy;' in s, 'extern dla wifiZbudujJsonStatus()')
spr('pogoda_json_liczba_w_bloku(' in s, 'pomocnicza pogoda_json_liczba_w_bloku()')
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
if s15:
    h15 = s15[s15.index('R"PIECHTML(') + 11:s15.index(')PIECHTML";')]
    spr(html == h15, 'HTML strony WWW identyczny jak w 3.58.15')
bl = re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)
for i, b in enumerate(bl):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
echo "=== B. plik 3.58.16 = 3.58.15 + patch ==="
if [ -f ../../v3.58.15-bez-historia-csv/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35816_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.15-bez-historia-csv/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.15-do-v3.58.16.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.15 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
fi
echo "=== C. testy paczki 3.58.15 (A-C) na tym pliku ==="
T15=../../v3.58.15-bez-historia-csv/tests
if [ -x "$T15/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.16"/FIRMWARE_VERSION = "v3.58.15"/' "$CPP_ABS" > /tmp/jako_35815.cpp
  W=$(CPP_PATH=/tmp/jako_35815.cpp "$T15/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "ZALICZONY|DYM-SYM|SUWAK|SYM-OD-RAZU|WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne BLEDY: porownania SHA "X + patch = ten plik" (inny plik) i "C++ za HTML-em identyczne jak w 3.58.14"
  # (3.58.16 zmienia C++ - to wlasnie ta paczka); wszystko inne musi przejsc.
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.14" || true)
  if [ "$NB" = "$NDOP" ] && [ "$NDOP" -le 4 ]; then echo "    [OK] testy 3.58.15/3.58.14 zaliczone (pominieto tylko porownania SHA z patchem i 'C++ identyczne z 3.58.14' - nie dotycza tego pliku)"; else echo "    BLAD: testy 3.58.15"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

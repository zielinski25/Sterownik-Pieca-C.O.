#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.17 (POGODA-TEMP).
#   A. parsowanie Open-Meteo: prawdziwy kod pobierzPrognozeDeszczu() skompilowany g++
#      (test_pogoda_parsowanie.py, 6 odpowiedzi) - pogoda_temp (ujemna, dodatnia, 0.0,
#      nieobecna, null) + wszystko z 3.58.16; /api/status ma pole pogoda_temp (null = brak)
#      zaraz za wiatr_porywy; DESZCZ_URL prosi o temperature_2m; wersja; HTML bez zmian.
#   B. plik 3.58.17 = 3.58.16 + patch (SHA).
#   C. pelne testy paczki 3.58.16 (A-C, w tym 3.58.15/3.58.14) na tym pliku.
#   ./uruchom_testy.sh        CPP_PATH=... inny plik
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
BLEDY=0
echo "=== A. POGODA-TEMP: parsowanie Open-Meteo, pole /api/status, URL, wersja, WWW ==="
python3 test_pogoda_parsowanie.py "$CPP_ABS" || BLEDY=$((BLEDY+1))
python3 - "$CPP_ABS" ../../v3.58.16-pogoda-zewn/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile, os
s = open(sys.argv[1], encoding='utf-8').read()
s16 = open(sys.argv[2], encoding='utf-8').read() if os.path.exists(sys.argv[2]) else None
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.17";') == 1, 'FIRMWARE_VERSION = "v3.58.17"')
st = s[s.index('String wifiZbudujJsonStatus() {'):]
st = st[:st.index('\n}\n')]
spr('\\"pogoda_temp\\":' in st, '/api/status ma pole "pogoda_temp"')
i = st.index('wiatr_porywy'); j = st.index('pogoda_temp')
spr(0 < j - i < 600, 'pogoda_temp zaraz za wiatr_porywy')
spr('String(pogodaTemp, 1)' in st and 'F("null")' in st[j:j+400], 'liczba z 1 miejscem po przecinku albo null (brak danych)')
url = s[s.index('static const char* DESZCZ_URL ='):]
url = url[:url.index(';')]
spr('current=precipitation,weather_code,is_day,wind_gusts_10m,temperature_2m' in url, 'DESZCZ_URL: current=...,temperature_2m')
spr('minutely_15=precipitation,weather_code' in url and 'forecast_days=2' in url, 'DESZCZ_URL: reszta jak w 3.58.16')
spr(s.count('const float POGODA_TEMP_BRAK = -99.0f;') == 1 and s.count('float pogodaTemp = POGODA_TEMP_BRAK;') == 1, 'definicje pogodaTemp / POGODA_TEMP_BRAK')
spr('extern float pogodaTemp;' in s and 'extern const float POGODA_TEMP_BRAK;' in s, 'extern dla wifiZbudujJsonStatus()')
spr('"temperature_2m", fTemp' in s and 'pogodaTemp = nowaTemp;' in s, 'odczyt temperature_2m w pobierzPrognozeDeszczu() i przypisanie')
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
if s16:
    h16 = s16[s16.index('R"PIECHTML(') + 11:s16.index(')PIECHTML";')]
    spr(html == h16, 'HTML strony WWW identyczny jak w 3.58.16')
bl = re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)
for i, b in enumerate(bl):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
echo "=== B. plik 3.58.17 = 3.58.16 + patch ==="
if [ -f ../../v3.58.16-pogoda-zewn/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35817_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.16-pogoda-zewn/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.16-do-v3.58.17.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.16 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
fi
echo "=== C. testy paczki 3.58.16 (A-C) na tym pliku ==="
T16=../../v3.58.16-pogoda-zewn/tests
if [ -x "$T16/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.17"/FIRMWARE_VERSION = "v3.58.16"/' "$CPP_ABS" > /tmp/jako_35816.cpp
  W=$(CPP_PATH=/tmp/jako_35816.cpp "$T16/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "ZALICZONY|WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne BLEDY: porownania SHA "X + patch = ten plik" (inny plik), "C++ za HTML-em identyczne
  # jak w 3.58.14", "DESZCZ_URL: current=precipitation,weather_code,is_day,wind_gusts_10m" (3.58.17
  # dopisuje ,temperature_2m - test 3.58.16 szuka dokladnego ciagu) i zbiorcze "testy 3.58.1x".
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[45]|BLAD: DESZCZ_URL: current=precipitation,weather_code,is_day,wind_gusts_10m$" || true)
  if [ "$NB" = "$NDOP" ] && [ "$NDOP" -le 6 ]; then echo "    [OK] testy 3.58.16/3.58.15/3.58.14 zaliczone (pominieto tylko porownania SHA z patchem, 'C++ identyczne z 3.58.14' i dokladny ciag URL z 3.58.16 - nie dotycza tego pliku)"; else echo "    BLAD: testy 3.58.16"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.18 (PIERWSZA-PROGNOZA).
#   A. bramka startu: prawdziwe definicje licznikow/stalych i prawdziwy warunek z zadania TG
#      skompilowane g++ (32-bit jak ESP32 i 64-bit) - pierwsze pobranie 60 s (DESZCZ) / 120 s
#      (PROGNOZA), potem co 15 min, przepelnienie millis() bez wplywu; wersja; brak innych
#      zmian (plik = 3.58.17 poza tymi liniami); HTML bez zmian.
#   B. plik 3.58.18 = 3.58.17 + patch (SHA).
#   C. pelne testy paczki 3.58.17 (A-C, w tym 3.58.16/3.58.15/3.58.14) na tym pliku.
#   ./uruchom_testy.sh        CPP_PATH=... inny plik
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
BLEDY=0
echo "=== A. PIERWSZA-PROGNOZA: bramka startu, wersja, zakres zmian, WWW ==="
python3 test_bramka_startu.py "$CPP_ABS" || BLEDY=$((BLEDY+1))
python3 - "$CPP_ABS" ../../v3.58.17-pogoda-temp/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile, os, difflib
s = open(sys.argv[1], encoding='utf-8').read()
s17 = open(sys.argv[2], encoding='utf-8').read() if os.path.exists(sys.argv[2]) else None
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.18";') == 1, 'FIRMWARE_VERSION = "v3.58.18"')
spr(s.count('unsigned long owDeszczLastFetchMillis = 0UL - (OW_DESZCZ_FETCH_INTERVAL_MS - OW_DESZCZ_PIERWSZE_PO_MS);') == 1, 'owDeszczLastFetchMillis startuje cofniety o (15 min - 60 s)')
spr(s.count('unsigned long owPrognozaLastFetchMillis = 0UL - (OW_PROGNOZA_FETCH_INTERVAL_MS - OW_PROGNOZA_PIERWSZE_PO_MS);') == 1, 'owPrognozaLastFetchMillis startuje cofniety o (15 min - 120 s)')
spr('unsigned long owDeszczLastFetchMillis = 0;' not in s and 'unsigned long owPrognozaLastFetchMillis = 0;' not in s, 'nie ma juz startu od zera (15 min zwloki)')
# stale zdefiniowane PRZED uzyciem w inicjalizacji (inaczej kompilator ESP32 zglosi blad)
for st, zm in (('OW_DESZCZ_FETCH_INTERVAL_MS', 'owDeszczLastFetchMillis ='), ('OW_DESZCZ_PIERWSZE_PO_MS', 'owDeszczLastFetchMillis ='), ('OW_PROGNOZA_FETCH_INTERVAL_MS', 'owPrognozaLastFetchMillis ='), ('OW_PROGNOZA_PIERWSZE_PO_MS', 'owPrognozaLastFetchMillis =')):
    spr(s.index('static const unsigned long ' + st) < s.index('unsigned long ' + zm), st + ' zdefiniowana przed inicjalizacja licznika')
spr(s.count('static const unsigned long OW_DESZCZ_FETCH_INTERVAL_MS = 15UL * 60UL * 1000UL;') == 1 and s.count('static const unsigned long OW_PROGNOZA_FETCH_INTERVAL_MS = 15UL * 60UL * 1000UL;') == 1, 'interwal 15 min bez zmian')
if s17:
    # poza naglowkiem/HISTORIA/wersja wolno zmienic TYLKO linie przy tych dwoch licznikach
    d = [l for l in difflib.unified_diff(s17.splitlines(), s.splitlines(), lineterm='', n=0) if l[:1] in '+-' and l[:3] not in ('+++', '---')]
    kod = [l for l in d if not l[1:].startswith('//') and 'FIRMWARE_VERSION' not in l]
    dozw = ('owDeszczLastFetchMillis', 'owPrognozaLastFetchMillis', 'OW_DESZCZ_FETCH_INTERVAL_MS', 'OW_PROGNOZA_FETCH_INTERVAL_MS', 'OW_DESZCZ_PIERWSZE_PO_MS', 'OW_PROGNOZA_PIERWSZE_PO_MS')
    obce = [l for l in kod if not any(k in l for k in dozw)]
    for l in obce: print('        nieoczekiwana zmiana: ' + l)
    spr(not obce, 'poza komentarzami i wersja zmienione tylko linie licznikow Open-Meteo (%d linii kodu)' % len(kod))
    h17 = s17[s17.index('R"PIECHTML(') + 11:s17.index(')PIECHTML";')]
    html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
    spr(html == h17, 'HTML strony WWW identyczny jak w 3.58.17')
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
bl = re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)
for i, b in enumerate(bl):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
echo "=== B. plik 3.58.18 = 3.58.17 + patch ==="
if [ -f ../../v3.58.17-pogoda-temp/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35818_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.17-pogoda-temp/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.17-do-v3.58.18.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.17 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
fi
echo "=== C. testy paczki 3.58.17 (A-C) na tym pliku ==="
T17=../../v3.58.17-pogoda-temp/tests
if [ -x "$T17/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.18"/FIRMWARE_VERSION = "v3.58.17"/' "$CPP_ABS" > /tmp/jako_35817.cpp
  W=$(CPP_PATH=/tmp/jako_35817.cpp "$T17/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "ZALICZONY|WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne BLEDY: porownania SHA "X + patch = ten plik" (inny plik), "HTML identyczny" NIE jest
  # dopuszczalny (HTML bez zmian), "C++ za HTML-em identyczne jak w 3.58.14", dokladny ciag URL z 3.58.16
  # i zbiorcze "testy 3.58.1x".
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[456]|BLAD: DESZCZ_URL: current=precipitation,weather_code,is_day,wind_gusts_10m$" || true)
  if [ "$NB" = "$NDOP" ] && [ "$NDOP" -le 8 ]; then echo "    [OK] testy 3.58.17/3.58.16/3.58.15/3.58.14 zaliczone (pominieto tylko porownania SHA z patchem, 'C++ identyczne z 3.58.14' i dokladny ciag URL z 3.58.16 - nie dotycza tego pliku)"; else echo "    BLAD: testy 3.58.17"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

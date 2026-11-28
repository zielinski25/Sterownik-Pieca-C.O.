#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.19 (PLYNNY-DESZCZ).
#   A. parsowanie Open-Meteo + odliczanie: prawdziwy kod (globalne, deszczStanTeraz(), cialo
#      pobierzPrognozeDeszczu()) skompilowany g++ natywnie 64-bit i "jak ESP32" (32-bit) -
#      13 przypadkow (6 z 3.58.16/17 jak dotad + 7 nowych: przesuniecie w slocie z zegara,
#      schodzenie co minute, przejscie w "pada" i w stan 0, pobranie 15 min pozniej bez skoku,
#      zegar niezgodny, koniec <= 0, koniec poza oknem, przepelnienie millis());
#      /api/status uzywa deszczStanTeraz(); wersja; HTML bez zmian; node --check.
#   B. plik 3.58.19 = 3.58.18 + patch (SHA).
#   C. testy paczki 3.58.18 (A-C, w tym 3.58.17/3.58.16/...) na tym pliku. Stare harnessy
#      parsowania (3.58.16/3.58.17) NIE znaja millis()/zegara, ktore od 3.58.19 sa w tym
#      fragmencie kodu - ich "kompilacja harnessu" jest tolerowana (ich przypadki sa w A).
#   ./uruchom_testy.sh        CPP_PATH=... inny plik
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
BLEDY=0
echo "=== A. PLYNNY-DESZCZ: parsowanie + odliczanie, /api/status, wersja, WWW ==="
python3 test_pogoda_parsowanie.py "$CPP_ABS" || BLEDY=$((BLEDY+1))
python3 - "$CPP_ABS" ../../v3.58.18-pierwsza-prognoza/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import re, subprocess, sys, tempfile, os
s = open(sys.argv[1], encoding='utf-8').read()
s18 = open(sys.argv[2], encoding='utf-8').read() if os.path.exists(sys.argv[2]) else None
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.19";') == 1, 'FIRMWARE_VERSION = "v3.58.19"')
st = s[s.index('String wifiZbudujJsonStatus() {'):]
st = st[:st.index('\n}\n')]
spr('deszczStanTeraz(dStan, dMin);' in st and 'String(dStan)' in st and 'String(dMin)' in st, '/api/status: deszcz_stan/deszcz_min z deszczStanTeraz() (na biezaco)')
spr('String(deszczStan)' not in st and 'String(deszczMinuty)' not in st, '/api/status nie wystawia juz zamrozonych deszczStan/deszczMinuty')
spr(s.index('void deszczStanTeraz(int &stan, int &minuty);') < s.index('String wifiZbudujJsonStatus() {') < s.index('void deszczStanTeraz(int &stan, int &minuty) {'), 'deklaracja deszczStanTeraz() przed wifiZbudujJsonStatus(), definicja przy pobierzPrognozeDeszczu()')
for g in ('unsigned long deszczStartMillis = 0;', 'unsigned long deszczKoniecMillis = 0;', 'bool deszczKoniecZnany = false;', 'int deszczKoniecMinuty = -1;'):
    spr(s.count(g) == 1, 'definicja: ' + g)
spr('if (g_rtcGotowy || g_rtcNtpZsynchronizowany) {' in s and 'aktualnyCzasGodzina * 60 + aktualnyCzasMinuta) - indeksTeraz * 15' in s, 'przesuniecie w slocie z zegara centrali tylko gdy RTC/NTP wiarygodne')
spr(s.index('int aktualnyCzasGodzina = 0;') < s.index('int przesMin = 0;') and s.index('bool g_rtcGotowy = false;') < s.index('int przesMin = 0;'), 'zegar i flagi RTC zdefiniowane przed uzyciem w pobierzPrognozeDeszczu()')
spr(s.count('deszczStan = nowyStan;') == 1 and s.count('deszczKoniecMinuty = nowyKoniecMinuty;') == 1 and s.count('deszczStartMillis = (nowyStan == 1) ? terazMs') == 1, 'chwile poczatku/konca ustawiane razem ze stanem, tylko po udanym pobraniu')
url = s[s.index('static const char* DESZCZ_URL ='):]; url = url[:url.index(';')]
if s18:
    u18 = s18[s18.index('static const char* DESZCZ_URL ='):]; u18 = u18[:u18.index(';')]
    spr(url == u18, 'DESZCZ_URL bez zmian (zapytanie do Open-Meteo jak w 3.58.18)')
    html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
    h18 = s18[s18.index('R"PIECHTML(') + 11:s18.index(')PIECHTML";')]
    spr(html == h18, 'HTML strony WWW identyczny jak w 3.58.18')
html = s[s.index('R"PIECHTML(') + 11:s.index(')PIECHTML";')]
bl = re.findall(r'<script(?![^>]*src=)[^>]*>(.*?)</script>', html, re.S)
for i, b in enumerate(bl):
    f = tempfile.NamedTemporaryFile('w', suffix='.js', delete=False); f.write(b); f.close()
    r = subprocess.run(['node', '--check', f.name], capture_output=True, text=True)
    spr(r.returncode == 0, 'blok <script> %d: node --check (%d znakow)' % (i, len(b)))
sys.exit(0 if ok else 1)
PY
echo "=== B. plik 3.58.19 = 3.58.18 + patch ==="
if [ -f ../../v3.58.18-pierwsza-prognoza/src/main_centrala.cpp ]; then
  ROB="${TMPDIR:-/tmp}/centrala_35819_test"; rm -rf "$ROB"; mkdir -p "$ROB"
  cp ../../v3.58.18-pierwsza-prognoza/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.18-do-v3.58.19.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.18 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
fi
echo "=== C. testy paczki 3.58.18 (A-C) na tym pliku ==="
T18=../../v3.58.18-pierwsza-prognoza/tests
if [ -x "$T18/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.19"/FIRMWARE_VERSION = "v3.58.18"/' "$CPP_ABS" > /tmp/jako_35818.cpp
  W=$(CPP_PATH=/tmp/jako_35818.cpp "$T18/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "ZALICZONY|WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne BLEDY: SHA po patchu (inny plik), stare harnessy parsowania (brak millis()/zegara),
  # "zmienione tylko linie licznikow" z 3.58.18 (ten plik zmienia wiecej), "C++ za HTML-em identyczne
  # jak w 3.58.14", dokladny ciag URL z 3.58.16, zbiorcze "testy 3.58.1x".
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: kompilacja harnessu|BLAD: poza komentarzami i wersja zmienione tylko|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[4567]|BLAD: DESZCZ_URL: current=precipitation,weather_code,is_day,wind_gusts_10m$" || true)
  if [ "$NB" = "$NDOP" ] && [ "$NDOP" -le 12 ]; then echo "    [OK] testy 3.58.18/3.58.17/3.58.16/3.58.15/3.58.14 zaliczone (pominieto: SHA z patchem, stare harnessy parsowania bez millis()/zegara - ich przypadki sa w A, 'tylko liczniki' z 3.58.18, 'C++ identyczne z 3.58.14', dokladny URL z 3.58.16)"; else echo "    BLAD: testy 3.58.18"; BLEDY=$((BLEDY+1)); fi
fi
echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

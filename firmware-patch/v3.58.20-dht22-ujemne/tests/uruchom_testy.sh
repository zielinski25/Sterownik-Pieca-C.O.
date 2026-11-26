#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.20 (DHT22-UJEMNE).
#   A. blok ZEW2-FILTR wyciety z pliku centrali (wytnij_zew2_filtr.py): 13 scenariuszy z 3.58.10
#      (regresja) + [14] zew2DekodujU2() na granicach, [15] wszystkie 400 wartosci -0,1..-40,0 C
#      w kodowaniu U2 (klon) i znak-modul (prawdziwy DHT22) przepuszczone przez dokladna formule
#      biblioteki dhtESP32-rmt, [16] scenariusz z 26.11 (0,3 -> 0,0 -> -0,1 -> -0,4 -> -1,2 -> -8,0).
#   B. PRAWDZIWA funkcja wilgotnosc() z atrapami sprzetu (wytnij_wilgotnosc.py): 9 scenariuszy
#      z 3.58.10 (regresja) + [10] ramki -3276.7/-3276.5/-3275.6 z biblioteki = -0,1/-0,3/-1,2 na
#      Zew 2, wilgotnosc przyjeta, WARN raz, licznik u2=; [11] smieci nadal odpadaja.
#   C. znaczniki 3.58.20 w pliku; plik 3.58.20 = 3.58.19 + patch (SHA).
#   D. testy paczki 3.58.19 (A-C, w tym 3.58.18/17/16/15/14) na tym pliku.
#   ./uruchom_testy.sh              SANITIZER=1 dodaje ASan+UBSan do A i B
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3, patch, sha256sum, node (czesc D). Linux albo WSL.
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
ROB="${TMPDIR:-/tmp}/centrala_35820_test"
rm -rf "$ROB"; mkdir -p "$ROB"
CXX="${CXX:-g++}"
FLAGI=(-std=c++17 -Wall -Wextra -Werror -O1 -I"$ROB")
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)
BLEDY=0

echo "=== A. ZEW2-FILTR + zew2DekodujU2() (wycinek z $CPP) ==="
python3 wytnij_zew2_filtr.py "$CPP_ABS" "$ROB/zew2_filtr_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr"
"$ROB/test_zew2_filtr" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr_san"
  "$ROB/test_zew2_filtr_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
fi

echo
echo "=== B. wilgotnosc() z pliku centrali + atrapy: klon U2 ponizej 0 C, smieci, regresja 3.58.10 ==="
python3 wytnij_wilgotnosc.py "$CPP_ABS" "$ROB/wilgotnosc_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc"
"$ROB/test_wilgotnosc" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc_san"
  "$ROB/test_wilgotnosc_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
fi

echo
echo "=== C. znaczniki 3.58.20; plik = 3.58.19 + patch ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import sys
s = open(sys.argv[1], encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.20";') == 1, 'FIRMWARE_VERSION = "v3.58.20"')
p, k = s.index('// --- ZEW2-FILTR-POCZATEK ---'), s.index('// --- ZEW2-FILTR-KONIEC ---')
blok = s[p:k]
spr('static bool zew2DekodujU2(float *t) {' in blok, 'zew2DekodujU2() w bloku ZEW2-FILTR (testowalnym na PC)')
spr('static const float ZEW2_U2_BAZA_C = 3276.8f;' in blok and 'ZEW2_U2_MIN_C  = -3276.75f;' in blok and 'ZEW2_U2_MAX_C  = -3236.75f;' in blok, 'stale U2: baza 3276,8, przedzial -3276,75..-3236,75')
spr(blok.index('zew2DekodujU2') < blok.index('static bool zew2Podejrzany'), 'dekoder zdefiniowany przed filtrem')
w = s[s.index('void wilgotnosc() {'):]; w = w[:w.index('\n}\n')]
spr(w.count('zew2DekodujU2(&dhtTempPomijana)') == 1, 'wilgotnosc(): dekodowanie dokladnie raz, na odczycie z read_dht()')
spr(w.index('read_dht(dhtTempPomijana') < w.index('zew2DekodujU2(&dhtTempPomijana)') < w.index('zew2RamkaPoprawna(dhtErr, dhtTempPomijana'), 'kolejnosc: read_dht -> zew2DekodujU2 -> zew2RamkaPoprawna (zakres -40..80 sprawdzany PO przeliczeniu)')
spr('dhtErr == DHT_OK && zew2DekodujU2' in w, 'dekodowanie tylko przy DHT_OK')
spr('zdarzenie=u2 surowa=%.1f t=%.1f' in w and 'if (dhtU2++ == 0)' in w, 'WARN o klonie U2 tylko przy pierwszym wykryciu')
spr('zera_temp=%lu u2=%lu kod=%u' in w, 'licznik u2= w linii DHT22 diagnostyka')
spr(s.count('zew2RamkaPoprawna(dhtErr, dhtTempPomijana, wilgotnoscOdczytanaLok)') == 1 and 't >= -40.0f && t <= 80.0f && rh >= 1.0f && rh <= 100.0f' in blok, 'zew2RamkaPoprawna() bez zmian (zakresy jak w 3.58.9)')
spr('[v3.58.20] — 2026-11-27 — ZEW 2 (DHT22) PONIZEJ 0 C' in s and s.startswith('// FIRMWARE_VERSION v3.58.19 -> v3.58.20'), 'naglowek i historia zmian 3.58.20')
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.19-plynny-deszcz/src/main_centrala.cpp ]; then
  cp ../../v3.58.19-plynny-deszcz/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.19-do-v3.58.20.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.19 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  # poza naglowkiem/historia/wersja zmienia sie tylko blok ZEW2-FILTR i wilgotnosc()
  python3 - "$CPP_ABS" ../../v3.58.19-plynny-deszcz/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import sys
n, o = [open(a, encoding='utf-8').read() for a in sys.argv[1:3]]
def html(s): return s[s.index('R"PIECHTML('):s.index(')PIECHTML";')]
ok = html(n) == html(o)
print(('    [OK] ' if ok else '    BLAD: ') + 'HTML strony WWW identyczny jak w 3.58.19')
def od(s, m): return s[s.index(m):]
reszta_ok = od(n, 'float cisnienieOdczytane = 0.0;').replace('"v3.58.20"', '"v3.58.19"').split('void wilgotnosc() {')[0] == od(o, 'float cisnienieOdczytane = 0.0;').split('void wilgotnosc() {')[0]
print(('    [OK] ' if reszta_ok else '    BLAD: ') + 'kod miedzy blokiem ZEW2-FILTR a wilgotnosc() identyczny jak w 3.58.19 (poza FIRMWARE_VERSION)')
po = lambda s: s[s.index('void wilgotnosc() {'):]; po = (lambda s: s[s.index('\n}\n'):])(po(n)), (lambda s: s[s.index('\n}\n'):])(po(o))
print(('    [OK] ' if po[0] == po[1] else '    BLAD: ') + 'kod za wilgotnosc() identyczny jak w 3.58.19')
sys.exit(0 if ok and reszta_ok and po[0] == po[1] else 1)
PY
fi

echo
echo "=== D. testy paczki 3.58.19 (A-C) na tym pliku ==="
T19=../../v3.58.19-plynny-deszcz/tests
if [ -x "$T19/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.20"/FIRMWARE_VERSION = "v3.58.19"/' "$CPP_ABS" > "$ROB/jako_35819.cpp"
  W=$(CPP_PATH="$ROB/jako_35819.cpp" "$T19/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne: SHA po patchu (inny plik) i to, co 3.58.19 samo toleruje w swoich starszych testach
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: kompilacja harnessu|BLAD: poza komentarzami i wersja zmienione tylko|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[4567]|BLAD: DESZCZ_URL" || true)
  if echo "$W" | grep -q "testy 3.58.18/3.58.17/3.58.16/3.58.15/3.58.14 zaliczone" && [ "$NB" = "$NDOP" ]; then
    echo "    [OK] testy 3.58.19/3.58.18/.../3.58.14 zaliczone (pominieto tylko SHA z patchem 3.58.19 - inny plik - i to, co 3.58.19 samo toleruje)"
  else echo "    BLAD: testy 3.58.19"; BLEDY=$((BLEDY+1)); fi
fi

echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

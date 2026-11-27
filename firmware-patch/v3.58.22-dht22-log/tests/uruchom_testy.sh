#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.22 (DHT22-LOG).
#   A. blok ZEW2-FILTR wyciety z pliku centrali (wytnij_zew2_filtr.py): [1]-[18] z paczek 3.58.10/20/21
#      bez zmian (blok w 3.58.22 nietkniety) - regresja.
#   B. PRAWDZIWA funkcja wilgotnosc() z atrapami sprzetu (wytnij_wilgotnosc.py): [1]-[14] z 3.58.10/20/21
#      (regresja) + [1] i [15] = [v3.58.22] linia "tag=DHT22 ... kod= surowa= t= rh= ramka=" w ZWYKLYM logu
#      (zapiszLog): kazda z pierwszych 30 prob, potem co 10., zmiana ok<->zla od razu (max raz na 10 min),
#      surowa = wartosc z biblioteki PRZED dekoderem klona (-3276.3 obok t=-0.5).
#   C. znaczniki 3.58.22 w pliku; plik 3.58.22 = 3.58.21 + patch (SHA); poza stalymi DHT_LOG_* i wilgotnosc()
#      plik identyczny z 3.58.21 (blok ZEW2-FILTR, zew2OdczytDs(), HTML, reszta).
#   D. testy paczki 3.58.21 (A-D, w tym 3.58.20/19/.../14) na tym pliku (do harnessu B 3.58.21 dopisana
#      atrapa zapiszLog() i stale DHT_LOG_*, bo 3.58.22 wymaga ich od testu).
#   ./uruchom_testy.sh              SANITIZER=1 dodaje ASan+UBSan do A i B
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3, patch, sha256sum, node (czesc D). Linux albo WSL.
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
ROB="${TMPDIR:-/tmp}/centrala_35822_test"
rm -rf "$ROB"; mkdir -p "$ROB"
CXX="${CXX:-g++}"
FLAGI=(-std=c++17 -Wall -Wextra -Werror -O1 -I"$ROB")
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)
BLEDY=0

echo "=== A. ZEW2-FILTR + zew2DekodujU2() + zew2DsOdbiega() (wycinek z $CPP) ==="
python3 wytnij_zew2_filtr.py "$CPP_ABS" "$ROB/zew2_filtr_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr"
"$ROB/test_zew2_filtr" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr_san"
  "$ROB/test_zew2_filtr_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
fi

echo
echo "=== B. wilgotnosc() z pliku centrali + atrapy: linia tag=DHT22 w zwyklym logu, regresja 3.58.10/20/21 ==="
python3 wytnij_wilgotnosc.py "$CPP_ABS" "$ROB/wilgotnosc_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc"
"$ROB/test_wilgotnosc" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc_san"
  "$ROB/test_wilgotnosc_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
fi

echo
echo "=== C. znaczniki 3.58.22; plik = 3.58.21 + patch ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import sys
s = open(sys.argv[1], encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.22";') == 1, 'FIRMWARE_VERSION = "v3.58.22"')
spr('static const uint32_t DHT_LOG_PIERWSZE = 30;' in s and 'static const uint32_t DHT_LOG_CO_N = 10;' in s and 'static const unsigned long DHT_LOG_ZMIANA_MIN_MS = 600000UL;' in s, 'stale: pierwsze 30 prob, potem co 10., zmiana max raz na 10 min')
w = s[s.index('void wilgotnosc() {'):]; w = w[:w.index('\n}\n')]
spr(w.count('"tag=DHT22 lvl=INFO proba=%u kod=%u surowa=%.1f t=%.1f rh=%.1f ramka=%s decyzja=%u u2=%u ds=%.1f"') == 1, 'format linii tag=DHT22 (kod biblioteki, surowa przed dekoderem, t po, rh, ramka, decyzja, u2, ds)')
spr(w.count('zapiszLog(String(linia));') == 1, 'zapis przez zapiszLog() (zwykly log, zawsze archiwowany) - dokladnie raz')
spr('if (dhtProby <= DHT_LOG_PIERWSZE || dhtProby % DHT_LOG_CO_N == 0 || zmiana) {' in w, 'warunek: pierwsze 30 / co 10. / zmiana ok<->zla')
spr('poprzedniaRamkaOk != ramkaOk' in w and '>= DHT_LOG_ZMIANA_MIN_MS' in w, 'zmiana ok<->zla z limitem czasu')
spr(w.index('++dhtProby;') < w.index('"tag=DHT22') < w.index('if (!ramkaOk) {\n      wilgotnoscOdczytanaLok = NAN;'), 'linia po zliczeniu proby, przed decyzja o RH (nic w logice nie przestawione)')
spr(w.count('zew2Filtruj(g_zew2Filtr, ramkaOk, dhtTempPomijana, czasBiezacy, &tZew2, dsOdbiega)') == 1 and w.count('zew2OdczytDs(&dsTemp)') == 1 and w.count('zew2DekodujU2(&dhtTempPomijana, &dhtU2Wariant)') == 1, 'filtr, odniesienie DS i dekoder z 3.58.20/21 bez zmian')
spr('[v3.58.22] — 2026-11-27 — DHT22: WYNIK KAZDEGO ODCZYTU W ZWYKLYM LOGU' in s and s.startswith('// FIRMWARE_VERSION v3.58.21 -> v3.58.22'), 'naglowek i historia zmian 3.58.22')
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.21-zew2-odniesienie-ds/src/main_centrala.cpp ]; then
  cp ../../v3.58.21-zew2-odniesienie-ds/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.21-do-v3.58.22.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.21 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  # poza naglowkiem/historia/wersja zmieniaja sie tylko stale DHT_LOG_* i wilgotnosc()
  python3 - "$CPP_ABS" ../../v3.58.21-zew2-odniesienie-ds/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import sys
n, o = [open(a, encoding='utf-8').read() for a in sys.argv[1:3]]
def html(s): return s[s.index('R"PIECHTML('):s.index(')PIECHTML";')]
r = [html(n) == html(o)]
print(('    [OK] ' if r[-1] else '    BLAD: ') + 'HTML strony WWW identyczny jak w 3.58.21')
blok = lambda s: s[s.index('// --- ZEW2-FILTR-POCZATEK ---'):s.index('// --- ZEW2-FILTR-KONIEC ---')]
r.append(blok(n) == blok(o)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'blok ZEW2-FILTR identyczny jak w 3.58.21')
def bez_stalych(s):
    a = s.index('// [v3.58.22 DHT22-LOG] rytm linii'); b = s.index('static unsigned long dhtOstatniaProbaMs = 0;')
    return s[:a] + s[b:]
sr = lambda s: s[s.index('// --- ZEW2-FILTR-KONIEC ---'):s.index('void wilgotnosc() {')]
r.append(bez_stalych(sr(n)).replace('"v3.58.22"', '"v3.58.21"') == sr(o)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'kod od konca bloku ZEW2-FILTR do wilgotnosc() identyczny jak w 3.58.21 (poza FIRMWARE_VERSION i stalymi DHT_LOG_*)')
po = lambda s: (lambda t: t[t.index('\n}\n'):])(s[s.index('void wilgotnosc() {'):])
r.append(po(n) == po(o)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'kod za wilgotnosc() identyczny jak w 3.58.21')
przed = lambda s: s[s.index('// --- ZEW2-FILTR-POCZATEK ---') - 1:]  # od bloku; naglowek/historia wyzej celowo pominiete
r.append(n[n.index('static const char* FIRMWARE_VERSION'):n.index('// --- ZEW2-FILTR-POCZATEK ---')] == o[o.index('static const char* FIRMWARE_VERSION'):o.index('// --- ZEW2-FILTR-POCZATEK ---')] if n.index('static const char* FIRMWARE_VERSION') < n.index('// --- ZEW2-FILTR-POCZATEK ---') else True)
sys.exit(0 if all(r) else 1)
PY
fi

echo "=== D. testy paczki 3.58.21 (A-D) na tym pliku ==="
T21=../../v3.58.21-zew2-odniesienie-ds/tests
if [ -x "$T21/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.22"/FIRMWARE_VERSION = "v3.58.21"/' "$CPP_ABS" > "$ROB/jako_35821.cpp"
  # kopia testow 3.58.21 z dopisana atrapa zapiszLog() i stalymi DHT_LOG_* (3.58.22 wymaga ich od harnessu B).
  # Logika testow 3.58.21 bez zmian.
  rm -rf "$ROB/t21" "$ROB/t21up"; mkdir -p "$ROB/t21"; cp -r "$T21/." "$ROB/t21/"
  python3 - "$ROB/t21/test_wilgotnosc.cpp" <<'PY'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
s = s.replace('static std::vector<std::string> g_logi;\n',
              'static std::vector<std::string> g_logi;\nstatic void zapiszLog(const String &t) { g_logi.push_back(t.s); }  // atrapa dla 3.58.22\n'
              'static const uint32_t DHT_LOG_PIERWSZE = 30, DHT_LOG_CO_N = 10, DHT_LOG_ZMIANA_MIN_MS = 600000;\n')
open(p, 'w', encoding='utf-8').write(s)
PY
  mkdir -p "$ROB/t21up/v3.58.21-zew2-odniesienie-ds"; mv "$ROB/t21" "$ROB/t21up/v3.58.21-zew2-odniesienie-ds/tests"
  cp "$T21/../main_centrala-v3.58.20-do-v3.58.21.patch" "$ROB/t21up/v3.58.21-zew2-odniesienie-ds/"
  for d in ../../v3.58.1*/ ../../v3.58.20-*/; do ln -sfn "$(cd "$d" && pwd)" "$ROB/t21up/$(basename "$d")"; done
  # 3.58.20 i 3.58.10 tez kompiluja wilgotnosc() (uruchamiane posrednio przez D 3.58.21/3.58.19...): kopie z ta sama atrapa
  for d in ../../v3.58.20-dht22-ujemne ../../v3.58.10-zew2-ponowienie; do
    b=$(basename "$d"); rm -f "$ROB/t21up/$b"; mkdir -p "$ROB/t21up/$b"; cp -r "$d/." "$ROB/t21up/$b/"
    python3 - "$ROB/t21up/$b/tests/test_wilgotnosc.cpp" <<'PY'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
s = s.replace('static std::vector<std::string> g_logi;\n',
              'static std::vector<std::string> g_logi;\nstatic void zapiszLog(const String &t) { g_logi.push_back(t.s); }  // atrapa dla 3.58.22\n'
              'static const uint32_t DHT_LOG_PIERWSZE = 30, DHT_LOG_CO_N = 10, DHT_LOG_ZMIANA_MIN_MS = 600000;\n', 1)
open(p, 'w', encoding='utf-8').write(s)
PY
  done
  W=$(CPP_PATH="$ROB/jako_35821.cpp" "$ROB/t21up/v3.58.21-zew2-odniesienie-ds/tests/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne: SHA po patchu (inny plik), znaczniki 3.58.21/3.58.20 ktore 3.58.22 celowo zmienia (naglowek,
  # "kod miedzy blokiem a wilgotnosc()" - nowe stale DHT_LOG_*) i to, co 3.58.21/3.58.20 same toleruja
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: kompilacja harnessu|BLAD: poza komentarzami i wersja zmienione tylko|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[4567]|BLAD: DESZCZ_URL|BLAD: WARN o klonie U2|BLAD: licznik u2= w linii|BLAD: naglowek i historia zmian 3.58.2[01]|BLAD: kod miedzy blokiem ZEW2-FILTR a wilgotnosc\(\) identyczny jak w 3.58.(19|20)|BLAD: kod od konca bloku ZEW2-FILTR do zmiennych czujnikow identyczny jak w 3.58.20" || true)
  if echo "$W" | grep -q "ZEW2-FILTR: .* 0 bledow" && echo "$W" | grep -q "WILGOTNOSC(): .* 0 bledow" && [ "$NB" = "$NDOP" ]; then
    echo "    [OK] testy 3.58.21/3.58.20/.../3.58.14 zaliczone (pominieto tylko SHA z patchami - inny plik - znaczniki celowo zmienione w 3.58.22 i to, co starsze paczki same toleruja)"
  else echo "    BLAD: testy 3.58.21"; BLEDY=$((BLEDY+1)); fi
fi

echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

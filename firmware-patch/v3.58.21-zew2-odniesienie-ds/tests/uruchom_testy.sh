#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.21 (ZEW2-ODNIESIENIE-DS).
#   A. blok ZEW2-FILTR wyciety z pliku centrali (wytnij_zew2_filtr.py): [1]-[16] z paczek 3.58.10/3.58.20
#      (regresja) + [17] zew2DsOdbiega(): prog 8 C, bez DS nigdy, NaN nigdy; [18] zew2Filtruj() z wymuszonym
#      "podejrzany" (DS odbiega): powtorka za 3 s, dwa zgodne odczyty i tak wygrywaja, domyslnie jak 3.58.20.
#   B. PRAWDZIWA funkcja wilgotnosc() z atrapami sprzetu (wytnij_wilgotnosc.py): [1]-[12] z 3.58.10/3.58.20
#      (regresja, atrapa DS = brak) + [13] DS swiezy: przeliczona ramka blisko DS przyjeta od razu, ds_roznica=
#      w logu, zwykla ramka daleko od DS NIE jest sprawdzana; [14] DS +10, ramka klona -0,3: powtorka zamiast
#      przyjecia + WARN ds_odniesienie, dwa zgodne odczyty potwierdzone mimo DS, bez DS jak 3.58.20.
#   C. znaczniki 3.58.21 w pliku; plik 3.58.21 = 3.58.20 + patch (SHA); poza blokiem ZEW2-FILTR, zew2OdczytDs()
#      i wilgotnosc() plik identyczny z 3.58.20.
#   D. testy paczki 3.58.20 (A-D, w tym 3.58.19/18/17/16/15/14) na tym pliku (do harnessu B 3.58.20
#      dopisana atrapa zew2OdczytDs(), bo 3.58.21 wymaga jej od testu).
#   ./uruchom_testy.sh              SANITIZER=1 dodaje ASan+UBSan do A i B
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3, patch, sha256sum, node (czesc D). Linux albo WSL.
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
ROB="${TMPDIR:-/tmp}/centrala_35821_test"
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
echo "=== B. wilgotnosc() z pliku centrali + atrapy: odniesienie DS18B20, klon U2, smieci, regresja 3.58.10/3.58.20 ==="
python3 wytnij_wilgotnosc.py "$CPP_ABS" "$ROB/wilgotnosc_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc"
"$ROB/test_wilgotnosc" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_wilgotnosc.cpp -o "$ROB/test_wilgotnosc_san"
  "$ROB/test_wilgotnosc_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
fi

echo
echo "=== C. znaczniki 3.58.21; plik = 3.58.20 + patch ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import sys
s = open(sys.argv[1], encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.21";') == 1, 'FIRMWARE_VERSION = "v3.58.21"')
p, k = s.index('// --- ZEW2-FILTR-POCZATEK ---'), s.index('// --- ZEW2-FILTR-KONIEC ---')
blok = s[p:k]
spr('static const float ZEW2_DS_PROG_C = 8.0f;' in blok and 'static bool zew2DsOdbiega(float tDht, float tDs, bool dsOk) {' in blok, 'prog 8 C i zew2DsOdbiega() w bloku ZEW2-FILTR (testowalnym na PC)')
spr('if (!dsOk || !isfinite(tDht) || !isfinite(tDs)) return false;' in blok and 'return fabsf(tDht - tDs) > ZEW2_DS_PROG_C;' in blok, 'zew2DsOdbiega(): bez DS / NaN nigdy, inaczej |roznica| > prog')
spr('float *wynik, bool wymusPodejrzany = false) {' in blok and 'const bool podejrzany = wymusPodejrzany || zew2Podejrzany(t, f.ostatniaDobra);' in blok, 'zew2Filtruj(): parametr wymusPodejrzany (domyslnie false = zachowanie 3.58.20)')
spr(blok.count('wymusPodejrzany') == 3, 'wymusPodejrzany uzyty tylko do flagi "podejrzany" (nic wiecej w filtrze nie ruszone)')
spr('static bool zew2DekodujU2(float *t, uint8_t *wariant = nullptr) {' in blok and 'ZEW2_U2_BAZA_C   = 3276.8f;' in blok and 'ZEW2_U2_12_BAZA_C = 409.6f;' in blok, 'dekoder U2 z 3.58.20 bez zmian')
h = s[s.index('static bool zew2OdczytDs(float *tDs) {'):s.index('void wilgotnosc() {')]
spr('telemetryQuality(TELEMETRY_CH_ZEWN, telemetryMonotonicMs()) != 0) return false;' in h and 'if (m.lastGoodSimulated || trybSymulacji) return false;' in h and '*tDs = temperaturaNaZewnatrz;' in h, 'zew2OdczytDs(): tylko swiezy (telemetryQuality == 0), niesymulowany DS18B20 = temperaturaNaZewnatrz')
w = s[s.index('void wilgotnosc() {'):]; w = w[:w.index('\n}\n')]
spr(w.count('zew2OdczytDs(&dsTemp)') == 1 and w.count('zew2DsOdbiega(dhtTempPomijana, dsTemp, dsOk)') == 1, 'wilgotnosc(): odczyt DS i porownanie dokladnie raz')
spr('const bool dsOdbiega = dhtPrzeliczona && zew2DsOdbiega(' in w, 'porownanie z DS TYLKO dla ramek przeliczonych z kodowania klona (dhtPrzeliczona)')
spr(w.index('zew2DekodujU2(&dhtTempPomijana, &dhtU2Wariant)') < w.index('zew2DsOdbiega(') < w.index('zew2Filtruj(g_zew2Filtr, ramkaOk, dhtTempPomijana, czasBiezacy, &tZew2, dsOdbiega)'), 'kolejnosc: dekoder -> odniesienie DS -> zew2Filtruj(..., dsOdbiega)')
spr('zdarzenie=ds_odniesienie t=%.1f ds=%.1f prog=%.0f' in w and '++dhtDsOdbiega;' in w, 'WARN ds_odniesienie + licznik')
spr('u2=%lu ds_roznica=%.1f ds_odbiega=%lu kod=%u' in w and '(dsOk && ramkaOk) ? (dsTemp - dhtTempPomijana) : NAN' in w, 'ds_roznica= (DS - DHT, nan bez DS) i ds_odbiega= w linii DHT22 diagnostyka')
spr('dhtPrzeliczona && dhtU2++ == 0' in w and 'zdarzenie=u2 wariant=%u surowa=%.1f t=%.1f' in w, 'WARN o klonie U2 z 3.58.20 nadal tylko raz')
spr(s.count('zew2RamkaPoprawna(dhtErr, dhtTempPomijana, wilgotnoscOdczytanaLok)') == 1 and 't >= -40.0f && t <= 80.0f && rh >= 1.0f && rh <= 100.0f' in blok, 'zew2RamkaPoprawna() bez zmian')
spr('[v3.58.21] — 2026-11-27 — ZEW 2: ODNIESIENIE DO DS18B20' in s and s.startswith('// FIRMWARE_VERSION v3.58.20 -> v3.58.21'), 'naglowek i historia zmian 3.58.21')
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.20-dht22-ujemne/src/main_centrala.cpp ]; then
  cp ../../v3.58.20-dht22-ujemne/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.20-do-v3.58.21.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.20 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  # poza naglowkiem/historia/wersja zmienia sie tylko blok ZEW2-FILTR, zew2OdczytDs() i wilgotnosc()
  python3 - "$CPP_ABS" ../../v3.58.20-dht22-ujemne/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import sys
n, o = [open(a, encoding='utf-8').read() for a in sys.argv[1:3]]
def html(s): return s[s.index('R"PIECHTML('):s.index(')PIECHTML";')]
ok = html(n) == html(o)
print(('    [OK] ' if ok else '    BLAD: ') + 'HTML strony WWW identyczny jak w 3.58.20')
def od(s, m): return s[s.index(m):]
def bez_helpera(s):
    a = s.index('// [v3.58.21 ZEW2-ODNIESIENIE-DS] DS18B20 (Zew 1) jako odniesienie'); b = s.index('void wilgotnosc() {')
    return s[:a] + s[b:]
reszta_ok = bez_helpera(od(n, 'float cisnienieOdczytane = 0.0;')).replace('"v3.58.21"', '"v3.58.20"').split('void wilgotnosc() {')[0] == od(o, 'float cisnienieOdczytane = 0.0;').split('void wilgotnosc() {')[0]
print(('    [OK] ' if reszta_ok else '    BLAD: ') + 'kod miedzy blokiem ZEW2-FILTR a wilgotnosc() identyczny jak w 3.58.20 (poza FIRMWARE_VERSION i nowa zew2OdczytDs())')
po = lambda s: s[s.index('void wilgotnosc() {'):]; po = (lambda s: s[s.index('\n}\n'):])(po(n)), (lambda s: s[s.index('\n}\n'):])(po(o))
print(('    [OK] ' if po[0] == po[1] else '    BLAD: ') + 'kod za wilgotnosc() identyczny jak w 3.58.20')
przed = lambda s: s[s.index('// --- ZEW2-FILTR-KONIEC ---'):s.index('float cisnienieOdczytane = 0.0;')]
print(('    [OK] ' if przed(n) == przed(o) else '    BLAD: ') + 'kod od konca bloku ZEW2-FILTR do zmiennych czujnikow identyczny jak w 3.58.20')
sys.exit(0 if ok and reszta_ok and po[0] == po[1] and przed(n) == przed(o) else 1)
PY
fi

echo "=== D. testy paczki 3.58.20 (A-D) na tym pliku ==="
T20=../../v3.58.20-dht22-ujemne/tests
if [ -x "$T20/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.21"/FIRMWARE_VERSION = "v3.58.20"/' "$CPP_ABS" > "$ROB/jako_35820.cpp"
  # kopia testow 3.58.20 z dopisana atrapa zew2OdczytDs() (3.58.21 wymaga jej od harnessu B), uzyciem
  # zew2DsOdbiega() w harnessie A (inaczej -Werror=unused-function) i bez zapisu dokladnej linii
  # "u2=N kod=0" (w 3.58.21 miedzy u2= a kod= sa nowe pola ds_*). Logika testow 3.58.20 bez zmian.
  rm -rf "$ROB/t20" "$ROB/t20up"; mkdir -p "$ROB/t20"; cp -r "$T20/." "$ROB/t20/"
  python3 - "$ROB/t20/test_wilgotnosc.cpp" <<'PY'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
s = s.replace('static float obliczSkorygowanaWilgotnoscWzgledna(float abs, float) { return abs; }\n',
              'static float obliczSkorygowanaWilgotnoscWzgledna(float abs, float) { return abs; }\nstatic bool zew2OdczytDs(float *) { return false; }  // atrapa dla 3.58.21: brak DS\n')
s = s.replace('wLogu("u2=3 kod=0")', 'wLogu("u2=3 ")').replace('wLogu("u2=6 kod=0")', 'wLogu("u2=6 ")')
open(p, 'w', encoding='utf-8').write(s)
p = p.replace('test_wilgotnosc.cpp', 'test_zew2_filtr.cpp'); s = open(p, encoding='utf-8').read()
s = s.replace('#include "zew2_filtr_wycinek.h"\n', '#include "zew2_filtr_wycinek.h"\n[[maybe_unused]] static void uzyj35821() { (void)zew2DsOdbiega(0.f, 0.f, false); }  // -Werror=unused-function w wycinku 3.58.21\n')
open(p, 'w', encoding='utf-8').write(s)
PY
  mkdir -p "$ROB/t20up/v3.58.20-dht22-ujemne"; mv "$ROB/t20" "$ROB/t20up/v3.58.20-dht22-ujemne/tests"
  cp "$T20/../main_centrala-v3.58.19-do-v3.58.20.patch" "$ROB/t20up/v3.58.20-dht22-ujemne/"
  for d in ../../v3.58.1*/; do ln -sfn "$(cd "$d" && pwd)" "$ROB/t20up/$(basename "$d")"; done   # starsze paczki (3.58.19 i nizej) dla czesci D testow 3.58.20
  W=$(CPP_PATH="$ROB/jako_35820.cpp" "$ROB/t20up/v3.58.20-dht22-ujemne/tests/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne: SHA po patchu (inny plik), znaczniki 3.58.20 ktore 3.58.21 celowo zmienia (u2 WARN, linia diagnostyki,
  # naglowek, "kod miedzy blokiem a wilgotnosc()" - nowa zew2OdczytDs) i to, co 3.58.20 samo toleruje w starszych testach
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: kompilacja harnessu|BLAD: poza komentarzami i wersja zmienione tylko|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[4567]|BLAD: DESZCZ_URL|BLAD: WARN o klonie U2|BLAD: licznik u2= w linii|BLAD: naglowek i historia zmian 3.58.20|BLAD: kod miedzy blokiem ZEW2-FILTR a wilgotnosc\(\) identyczny jak w 3.58.19" || true)
  if echo "$W" | grep -q "ZEW2-FILTR: .* 0 bledow" && echo "$W" | grep -q "WILGOTNOSC(): .* 0 bledow" && echo "$W" | grep -q "testy 3.58.19/3.58.18/.../3.58.14 zaliczone" && [ "$NB" = "$NDOP" ]; then
    echo "    [OK] testy 3.58.20/3.58.19/.../3.58.14 zaliczone (pominieto tylko SHA z patchem 3.58.20 - inny plik - znaczniki 3.58.20 celowo zmienione w 3.58.21 i to, co 3.58.20 samo toleruje)"
  else echo "    BLAD: testy 3.58.20"; BLEDY=$((BLEDY+1)); fi
fi

echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

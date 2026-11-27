#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.22 (DHT22-LOG: kod i surowa wartosc DHT22 w logu i w telemetrii).
#   A. blok ZEW2-FILTR wyciety z pliku centrali (wytnij_zew2_filtr.py): [1]-[18] z paczek 3.58.10/20/21
#      bez zmian (blok w 3.58.22 nietkniety) - regresja.
#   B. PRAWDZIWA funkcja wilgotnosc() z atrapami sprzetu (wytnij_wilgotnosc.py): [1]-[14] z 3.58.10/20/21
#      (regresja) + [1] i [15] = [v3.58.22] linia "tag=DHT22 ... kod= surowa= t= rh= ramka=" w ZWYKLYM logu
#      (zapiszLog): kazda z pierwszych 30 prob, potem co 10., zmiana ok<->zla od razu (max raz na 10 min),
#      surowa = wartosc z biblioteki PRZED dekoderem klona (-3276.3 obok t=-0.5).
#   B2. PRAWDZIWA telemetria minutowa (wytnij_telemetria.py: TelemetrySample, telemetryAppendPathForSample(),
#      fragment telemetryBuildSnapshot()): [1]-[6] = pola "dht_kod"/"dht_surowa" w rekordzie Firebase, "sim"
#      maskowane do kanalow 0..10, stare probki/brak odczytu bez tych pol, ABI 80 B i offsety bez zmian.
#   C. znaczniki 3.58.22 w pliku; plik 3.58.22 = 3.58.21 + patch (SHA); po wycieciu 5 oznaczonych wstawek
#      [v3.58.22 ...] plik identyczny z 3.58.21 (blok ZEW2-FILTR, zew2OdczytDs(), HTML, reszta).
#   D. testy paczki 3.58.21 (A-D, w tym 3.58.20/19/.../14) na tym pliku (do harnessu B 3.58.21 dopisana
#      atrapa zapiszLog(), stale DHT_LOG_* i zmienne g_dhtOstatni*, bo 3.58.22 wymaga ich od testu; test
#      "kod za wilgotnosc() identyczny jak w 3.58.19/20" z definicji nie przechodzi - telemetria sie zmienila).
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
echo "=== B2. telemetria minutowa (Firebase) z pliku centrali: pola dht_kod / dht_surowa, maska sim, ABI 80 B ==="
python3 wytnij_telemetria.py "$CPP_ABS" "$ROB/telemetria_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_telemetria_dht.cpp -o "$ROB/test_telemetria_dht"
"$ROB/test_telemetria_dht" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_telemetria_dht.cpp -o "$ROB/test_telemetria_dht_san"
  "$ROB/test_telemetria_dht_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
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
spr('[v3.58.22] — 2026-11-27 — DHT22: KOD I SUROWA WARTOSC W ZWYKLYM LOGU I W TELEMETRII' in s and s.startswith('// FIRMWARE_VERSION v3.58.21 -> v3.58.22'), 'naglowek i historia zmian 3.58.22')
spr('g_dhtOstatniKod = dhtErr;' in w and 'g_dhtBylOdczyt = true;' in w and 'g_dhtOstatniaSurowaX10 = (dhtErr == DHT_OK && isfinite(dhtTempSurowa))' in w, 'wilgotnosc(): ostatni kod i surowa x10 (tylko przy kod 0) dla telemetrii')
spr(s.count('"\\"dht_kod\\":%u,"') == 1 and s.count('dodajTemp("dht_surowa", s.tempDiagReserved, -32767, 32767);') == 1 and s.count('(s.simulatedBits & TELEMETRY_SIM_CH_MASK)') == 1, 'telemetria: pola dht_kod / dht_surowa w rekordzie, "sim" = tylko kanaly 0..10')
spr('out.tempDiagReserved = g_dhtOstatniaSurowaX10;' in s and 'TELEMETRY_SIM_DHT_PRESENT = 0x8000U' in s and 'TELEMETRY_SIM_DHT_KOD_SHIFT = 11' in s and 'TELEMETRY_SIM_CH_MASK = 0x07FFU' in s, 'probka: surowa w tempDiagReserved, kod w bitach 11..14, znacznik bit 15 (ABI 80 B bez zmian)')
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
# wstawki 3.58.22 poza wilgotnosc() - kazda ma znacznik "[v3.58.22"; po ich usunieciu plik musi byc 1:1 jak 3.58.21
def wytnij(s, a, b, z_koncem=False):
    assert s.count(a) == 1, a
    i = s.index(a); j = s.index(b, i) + (len(b) if z_koncem else 0)
    return s[:i] + s[j:]
def bez_wstawek(s):
    s = wytnij(s, '// [v3.58.22 DHT22-LOG] simulatedBits: bity 0..10', 'static constexpr uint8_t  TELEMETRY_SIM_DHT_KOD_SHIFT = 11;\n', True)   # stale telemetrii
    s = wytnij(s, '// [v3.58.22 DHT22-LOG] rytm linii', 'static unsigned long dhtOstatniaProbaMs = 0;')                                   # DHT_LOG_* + g_dhtOstatni*
    s = wytnij(s, '  // [v3.58.22 DHT22-LOG] Ostatni odczyt DHT22 w probce', '  out.qualityBits |= TEMP_DIAG_MARK;')                    # telemetryBuildSnapshot()
    s = wytnij(s, '    // [v3.58.22 DHT22-LOG] ostatni odczyt DHT22: kod biblioteki', '  }\n  telemetryJsonAppend(buf, cap, used, "\\"age\\":[");')  # JSON dht_kod/dht_surowa
    a = '(unsigned)(s.simulatedBits & TELEMETRY_SIM_CH_MASK));   // [v3.58.22] bez bitow DHT'
    assert s.count(a) == 1; s = s.replace(a, '(unsigned)s.simulatedBits);')                                                             # maska "sim"
    return s.replace('"v3.58.22"', '"v3.58.21"')
nn = bez_wstawek(n)
sr = lambda s: s[s.index('// --- ZEW2-FILTR-KONIEC ---'):s.index('void wilgotnosc() {')]
po = lambda s: (lambda t: t[t.index('\n}\n'):])(s[s.index('void wilgotnosc() {'):])
r.append('[v3.58.22' not in sr(nn) and '[v3.58.22' not in po(nn)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'poza naglowkiem/historia i wilgotnosc() wszystkie wstawki 3.58.22 maja znacznik i sa znane (5 miejsc)')
r.append(sr(nn) == sr(o)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'kod od konca bloku ZEW2-FILTR do wilgotnosc() identyczny jak w 3.58.21 (poza FIRMWARE_VERSION, stalymi DHT_LOG_*/TELEMETRY_SIM_* i wstawka w telemetryBuildSnapshot())')
r.append(po(nn) == po(o)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'kod za wilgotnosc() identyczny jak w 3.58.21 (poza polami dht_kod/dht_surowa i maska "sim" w telemetryAppendPathForSample())')
r.append('static_assert(sizeof(TelemetrySample) == 80' in n and 'static_assert(offsetof(TelemetrySample, tempDiagReserved) == 78' in n); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'ABI probki telemetrii: static_assert 80 B i offset 78 nadal w pliku')
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
              'static const uint32_t DHT_LOG_PIERWSZE = 30, DHT_LOG_CO_N = 10, DHT_LOG_ZMIANA_MIN_MS = 600000;\n'
              'static volatile uint8_t g_dhtOstatniKod = 0; static volatile int16_t g_dhtOstatniaSurowaX10 = 0; static volatile bool g_dhtBylOdczyt = false;\n')
open(p, 'w', encoding='utf-8').write(s)
PY
  # 3.58.22 zmienia tez telemetrie za wilgotnosc() (dht_kod/dht_surowa) -> test "kod za wilgotnosc() identyczny
  # jak w 3.58.19" paczki 3.58.20 z definicji nie przejdzie; kopia 3.58.21 dopuszcza te linie (jak pozostale znane).
  python3 - "$ROB/t21/uruchom_testy.sh" <<'PY'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
a = 'identyczny jak w 3.58.19" || true)'
assert s.count(a) == 1
s = s.replace(a, 'identyczny jak w 3.58.19|BLAD: kod za wilgotnosc\\\\(\\\\) identyczny jak w 3.58.19" || true)')
open(p, 'w', encoding='utf-8').write(s)
PY
  grep -q 'kod za wilgotnosc' "$ROB/t21/uruchom_testy.sh" || { echo "    BLAD: nie udalo sie dopisac wyjatku do kopii testow 3.58.21"; BLEDY=$((BLEDY+1)); }
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
              'static const uint32_t DHT_LOG_PIERWSZE = 30, DHT_LOG_CO_N = 10, DHT_LOG_ZMIANA_MIN_MS = 600000;\n'
              'static volatile uint8_t g_dhtOstatniKod = 0; static volatile int16_t g_dhtOstatniaSurowaX10 = 0; static volatile bool g_dhtBylOdczyt = false;\n', 1)
open(p, 'w', encoding='utf-8').write(s)
PY
  done
  W=$(CPP_PATH="$ROB/jako_35821.cpp" "$ROB/t21up/v3.58.21-zew2-odniesienie-ds/tests/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "WSZYSTKO OK|BLEDOW|BLAD" | sed 's/^/    /'
  # dopuszczalne: SHA po patchu (inny plik), znaczniki 3.58.21/3.58.20 ktore 3.58.22 celowo zmienia (naglowek,
  # "kod miedzy blokiem a wilgotnosc()" - nowe stale DHT_LOG_*) i to, co 3.58.21/3.58.20 same toleruja
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD: SHA po patchu|BLAD: kompilacja harnessu|BLAD: poza komentarzami i wersja zmienione tylko|BLAD: C\+\+ za HTML-em identyczne|BLAD: testy 3.58.1[4567]|BLAD: DESZCZ_URL|BLAD: WARN o klonie U2|BLAD: licznik u2= w linii|BLAD: naglowek i historia zmian 3.58.2[01]|BLAD: kod miedzy blokiem ZEW2-FILTR a wilgotnosc\(\) identyczny jak w 3.58.(19|20)|BLAD: kod od konca bloku ZEW2-FILTR do zmiennych czujnikow identyczny jak w 3.58.20|BLAD: kod za wilgotnosc\\(\\) identyczny jak w 3.58.(19|20)" || true)
  if echo "$W" | grep -q "ZEW2-FILTR: .* 0 bledow" && echo "$W" | grep -q "WILGOTNOSC(): .* 0 bledow" && [ "$NB" = "$NDOP" ]; then
    echo "    [OK] testy 3.58.21/3.58.20/.../3.58.14 zaliczone (pominieto tylko SHA z patchami - inny plik - znaczniki celowo zmienione w 3.58.22 i to, co starsze paczki same toleruja)"
  else echo "    BLAD: testy 3.58.21"; BLEDY=$((BLEDY+1)); fi
fi

echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

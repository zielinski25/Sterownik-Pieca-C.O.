#!/usr/bin/env bash
# Testy na PC firmware'u centrali 3.58.23 (DHT22-U2: kodowanie U2 16-bit potwierdzone, dekoder docelowy).
#   A. blok ZEW2-FILTR wyciety z pliku centrali (wytnij_zew2_filtr.py): [1]-[18] z paczek 3.58.10/20/21/22,
#      w tym [14]/[15] przerobione: U2 16-bit co do 0,1 C przez odtworzone slowo (w tym -3275.6 -> -1.2
#      z eksportu 27.11), dawny wariant 12-bit (+369,6..+409,5) = zla ramka.
#   B. PRAWDZIWA funkcja wilgotnosc() z atrapami sprzetu (wytnij_wilgotnosc.py): [1]-[15] z 3.58.10/20/21/22;
#      [10] bez WARN o klonie, [12] +408.8/+407.9 = zla ramka (Zew 2 zostaje), [15] rytm linii tag=DHT22:
#      pierwsze 15 prob + zmiana ok<->zla, bez "co 10.".
#   B2. telemetria minutowa (wytnij_telemetria.py): pola dht_kod/dht_surowa, maska sim, ABI 80 B - bez zmian od 3.58.22.
#   C. znaczniki 3.58.23 w pliku; plik 3.58.23 = 3.58.22 + patch (SHA); poza zew2DekodujU2(), 3 miejscami w
#      wilgotnosc() i stalymi DHT_LOG_* plik identyczny z 3.58.22 (reszta bloku ZEW2-FILTR, HTML, telemetria, reszta).
#   D. testy paczki 3.58.22 (A, B, B2, C-znaczniki) na tym pliku: kopia testow 3.58.22 z wywolaniami
#      zew2DekodujU2(&t, &w) -> zew2DekodujU2(&t) (parametr usuniety). Dopuszczalne sa WYLACZNIE niezaliczenia
#      wynikajace z celowych zmian 3.58.23 (lista ponizej); kazdy inny BLAD = regresja.
#   ./uruchom_testy.sh              SANITIZER=1 dodaje ASan+UBSan do A, B i B2
#   CPP_PATH=sciezka/main_centrala.cpp ./uruchom_testy.sh   inny plik centrali
# Wymaga: g++ (C++17), python3, patch, sha256sum. Linux albo WSL.
set -euo pipefail
cd "$(dirname "$0")"
CPP="${CPP_PATH:-../src/main_centrala.cpp}"
CPP_ABS="$(cd "$(dirname "$CPP")" && pwd)/$(basename "$CPP")"
ROB="${TMPDIR:-/tmp}/centrala_35823_test"
rm -rf "$ROB"; mkdir -p "$ROB"
CXX="${CXX:-g++}"
FLAGI=(-std=c++17 -Wall -Wextra -Werror -O1 -I"$ROB")
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)
BLEDY=0

echo "=== A. ZEW2-FILTR + zew2DekodujU2() [U2 16-bit] + zew2DsOdbiega() (wycinek z $CPP) ==="
python3 wytnij_zew2_filtr.py "$CPP_ABS" "$ROB/zew2_filtr_wycinek.h" | sed 's/^/    /'
"$CXX" "${FLAGI[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr"
"$ROB/test_zew2_filtr" | sed 's/^/    /' || BLEDY=$((BLEDY+1))
if [ "${SANITIZER:-0}" = "1" ]; then
  "$CXX" "${FLAGI[@]}" "${SAN[@]}" test_zew2_filtr.cpp -o "$ROB/test_zew2_filtr_san"
  "$ROB/test_zew2_filtr_san" | tail -1 | sed 's/^/    [ASan+UBSan] /' || BLEDY=$((BLEDY+1))
fi

echo
echo "=== B. wilgotnosc() z pliku centrali + atrapy: dekoder U2, linia tag=DHT22, regresja 3.58.10/20/21/22 ==="
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
echo "=== C. znaczniki 3.58.23; plik = 3.58.22 + patch ==="
python3 - "$CPP_ABS" <<'PY' || BLEDY=$((BLEDY+1))
import sys
s = open(sys.argv[1], encoding='utf-8').read()
ok = True
def spr(w, co):
    global ok; print(('    [OK] ' if w else '    BLAD: ') + co); ok = ok and w
spr(s.count('static const char* FIRMWARE_VERSION = "v3.58.23";') == 1, 'FIRMWARE_VERSION = "v3.58.23"')
blok = s[s.index('// --- ZEW2-FILTR-POCZATEK ---'):s.index('// --- ZEW2-FILTR-KONIEC ---')]
spr(blok.count('static bool zew2DekodujU2(float *t) {') == 1 and 'wariant' not in blok.split('static bool zew2DekodujU2')[1].split('\n}\n')[0], 'zew2DekodujU2(float *t) - bez parametru wariant')
spr('const uint16_t slowo = (uint16_t)(0x8000U | ((uint16_t)lroundf(-*t * 10.0f) & 0x7FFFU));' in blok and 'const int16_t u2 = (int16_t)slowo;' in blok and 'if (u2 < -400 || u2 > -1) return false;' in blok and '*t = (float)u2 / 10.0f;' in blok, 'dekoder: odtworzone slowo 16-bit -> int16 -> /10, tylko -400..-1 (-40,0..-0,1 C)')
spr('ZEW2_U2_MIN_C = -3276.75f' in blok and 'ZEW2_U2_MAX_C = -3236.75f' in blok and 'ZEW2_U2_12' not in blok and 'ZEW2_U2_BAZA_C' not in blok and '409.6' not in blok and '369.55' not in blok, 'tylko obraz 16-bit (-3276,75..-3236,75); stale i kod wariantu 12-bit usuniete')
w = s[s.index('void wilgotnosc() {'):]; w = w[:w.index('\n}\n')]
spr(w.count('zew2DekodujU2(&dhtTempPomijana)') == 1 and 'dhtU2Wariant' not in w and 'zdarzenie=u2' not in w and 'if (dhtPrzeliczona) ++dhtU2;' in w, 'wilgotnosc(): dekoder bez wariantu, bez WARN "klon koduje ujemne w U2", licznik u2 zostaje')
spr('static const uint32_t DHT_LOG_PIERWSZE = 15;' in s and 'DHT_LOG_CO_N' not in s.replace('(bylo DHT_LOG_CO_N)', '') and 'static const unsigned long DHT_LOG_ZMIANA_MIN_MS = 600000UL;' in s, 'stale: pierwsze 15 prob, zmiana max raz na 10 min; DHT_LOG_CO_N usuniete')
spr('if (dhtProby <= DHT_LOG_PIERWSZE || zmiana) {' in w and '% DHT_LOG_CO_N' not in w, 'warunek linii tag=DHT22: pierwsze 15 / zmiana ok<->zla (bez "co 10.")')
spr(w.count('"tag=DHT22 lvl=INFO proba=%u kod=%u surowa=%.1f t=%.1f rh=%.1f ramka=%s decyzja=%u u2=%u ds=%.1f"') == 1 and w.count('zapiszLog(String(linia));') == 1, 'format linii tag=DHT22 i zapis przez zapiszLog() bez zmian')
spr(w.count('zew2Filtruj(g_zew2Filtr, ramkaOk, dhtTempPomijana, czasBiezacy, &tZew2, dsOdbiega)') == 1 and w.count('zew2OdczytDs(&dsTemp)') == 1 and 'g_dhtOstatniKod = dhtErr;' in w, 'filtr, odniesienie DS (3.58.21) i zapis kodu/surowej dla telemetrii (3.58.22) bez zmian')
spr(s.count('"\\"dht_kod\\":%u,"') == 1 and s.count('dodajTemp("dht_surowa", s.tempDiagReserved, -32767, 32767);') == 1, 'telemetria: pola dht_kod / dht_surowa bez zmian')
spr('[v3.58.23] — 2026-11-27 — DHT22: KODOWANIE U2 16-BIT POTWIERDZONE' in s and s.startswith('// FIRMWARE_VERSION v3.58.22 -> v3.58.23'), 'naglowek i historia zmian 3.58.23')
sys.exit(0 if ok else 1)
PY
if [ -f ../../v3.58.22-dht22-log/src/main_centrala.cpp ]; then
  cp ../../v3.58.22-dht22-log/src/main_centrala.cpp "$ROB/main_centrala.cpp"
  (cd "$ROB" && patch -s -p2 main_centrala.cpp < "$OLDPWD/../main_centrala-v3.58.22-do-v3.58.23.patch")
  A=$(sha256sum "$ROB/main_centrala.cpp" | cut -c1-64); B=$(sha256sum "$CPP_ABS" | cut -c1-64)
  if [ "$A" = "$B" ]; then echo "    [OK] 3.58.22 + patch = ten plik (SHA $B)"; else echo "    BLAD: SHA po patchu $A, plik $B"; BLEDY=$((BLEDY+1)); fi
  # poza naglowkiem/historia/wersja zmieniaja sie tylko: zew2DekodujU2() (blok ZEW2-FILTR), stale DHT_LOG_*
  # i trzy miejsca w wilgotnosc(). Reszta (HTML, telemetria, zew2OdczytDs(), cala reszta pliku) 1:1 jak 3.58.22.
  python3 - "$CPP_ABS" ../../v3.58.22-dht22-log/src/main_centrala.cpp <<'PY' || BLEDY=$((BLEDY+1))
import sys
n, o = [open(a, encoding='utf-8').read() for a in sys.argv[1:3]]
def html(s): return s[s.index('R"PIECHTML('):s.index(')PIECHTML";')]
r = [html(n) == html(o)]
print(('    [OK] ' if r[-1] else '    BLAD: ') + 'HTML strony WWW identyczny jak w 3.58.22')
def miedzy(s, a, b): return s[s.index(a):s.index(b)]
# blok ZEW2-FILTR: poza zew2DekodujU2() (od komentarza nad nia do zew2DsOdbiega) identyczny
blok = lambda s: miedzy(s, '// --- ZEW2-FILTR-POCZATEK ---', '// --- ZEW2-FILTR-KONIEC ---')
dek_n = miedzy(blok(n), '// [v3.58.23 DHT22-U2]', '// [v3.58.21 ZEW2-ODNIESIENIE-DS]')
dek_o = miedzy(blok(o), '// [v3.58.20 DHT22-UJEMNE] Klony DHT22', '// [v3.58.21 ZEW2-ODNIESIENIE-DS]')
r.append(blok(n).replace(dek_n, '') == blok(o).replace(dek_o, '')); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'blok ZEW2-FILTR poza zew2DekodujU2() identyczny jak w 3.58.22 (filtr, DS, ponowienia nietkniete)')
r.append(n[:n.index('// --- ZEW2-FILTR-POCZATEK ---')].count('[v3.58.23') == n[:n.index('// --- ZEW2-FILTR-POCZATEK ---')].count('[v3.58.23') and n[n.index('static const char* FIRMWARE_VERSION'):n.index('// --- ZEW2-FILTR-POCZATEK ---')].replace('"v3.58.23"', '"v3.58.22"') == o[o.index('static const char* FIRMWARE_VERSION'):o.index('// --- ZEW2-FILTR-POCZATEK ---')] if n.index('static const char* FIRMWARE_VERSION') < n.index('// --- ZEW2-FILTR-POCZATEK ---') else True)
# od konca bloku do wilgotnosc(): tylko stale DHT_LOG_*
sr = lambda s: miedzy(s, '// --- ZEW2-FILTR-KONIEC ---', 'void wilgotnosc() {')
st_n = miedzy(sr(n), '// [v3.58.22 DHT22-LOG] rytm linii', '// [v3.58.22 DHT22-LOG] Ostatni odczyt DHT22 dla telemetrii')
st_o = miedzy(sr(o), '// [v3.58.22 DHT22-LOG] rytm linii', '// [v3.58.22 DHT22-LOG] Ostatni odczyt DHT22 dla telemetrii')
r.append(sr(n).replace(st_n, '').replace('"v3.58.23"', '"v3.58.22"') == sr(o).replace(st_o, '')); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'kod od konca bloku ZEW2-FILTR do wilgotnosc() identyczny jak w 3.58.22 (poza FIRMWARE_VERSION i stalymi DHT_LOG_*)')
r.append('[v3.58.23' in st_n and 'DHT_LOG_PIERWSZE = 15;' in st_n and 'DHT_LOG_PIERWSZE = 30;' in st_o); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'stale DHT_LOG_*: 30 -> 15, bez DHT_LOG_CO_N (oznaczone [v3.58.23])')
# za wilgotnosc(): identyczne
po = lambda s: (lambda t: t[t.index('\n}\n'):])(s[s.index('void wilgotnosc() {'):])
r.append(po(n) == po(o)); print(('    [OK] ' if r[-1] else '    BLAD: ') + 'kod za wilgotnosc() identyczny jak w 3.58.22 (telemetria dht_kod/dht_surowa, Firebase, reszta)')
# wilgotnosc(): roznice tylko w 3 miejscach (dekoder+WARN, komentarz rytmu, warunek linii)
import difflib
wn = (lambda t: t[:t.index('\n}\n')])(n[n.index('void wilgotnosc() {'):]).split('\n')
wo = (lambda t: t[:t.index('\n}\n')])(o[o.index('void wilgotnosc() {'):]).split('\n')
hunki = [h for h in difflib.SequenceMatcher(None, wo, wn, autojunk=False).get_opcodes() if h[0] != 'equal']
usuniete = [l for h in hunki for l in wo[h[1]:h[2]]]
dodane = [l for h in hunki for l in wn[h[3]:h[4]]]
kod_usun = [l for l in usuniete if not l.strip().startswith('//')]
kod_dod = [l for l in dodane if not l.strip().startswith('//')]
r.append(len(kod_usun) == 7 and len(kod_dod) == 3 and all('DLOGF' in l or 'dhtU2Wariant' in l or 'zew2DekodujU2' in l or 'dhtU2++' in l or 'DHT_LOG_CO_N' in l or '(unsigned)dhtU2Wariant' in l or l.strip() in ('}',) for l in kod_usun) and all('zew2DekodujU2(&dhtTempPomijana)' in l or '++dhtU2' in l or 'DHT_LOG_PIERWSZE || zmiana' in l for l in kod_dod))
print(('    [OK] ' if r[-1] else '    BLAD: ') + 'wilgotnosc(): roznice wzgledem 3.58.22 tylko w 3 miejscach (dekoder bez wariantu + bez WARN, komentarz rytmu, warunek linii): -7/+3 linii kodu (jest -%d/+%d, %d hunkow z komentarzami)' % (len(kod_usun), len(kod_dod), len(hunki)))
sys.exit(0 if all(r) else 1)
PY
fi

echo
echo "=== D. testy paczki 3.58.22 (A, B, B2, C-znaczniki) na tym pliku ==="
T22=../../v3.58.22-dht22-log/tests
if [ -x "$T22/uruchom_testy.sh" ]; then
  sed 's/FIRMWARE_VERSION = "v3.58.23"/FIRMWARE_VERSION = "v3.58.22"/' "$CPP_ABS" > "$ROB/jako_35822.cpp"
  # kopia testow 3.58.22 BEZ katalogow sasiednich (C-SHA i D paczki 3.58.22 same sie pomijaja - sa wykonane
  # wyzej w innej postaci; scenariusze 3.58.21/20/10 sa w testach 3.58.22 w calosci). Jedyna zmiana w kopii:
  # zew2DekodujU2(&t, &w) -> zew2DekodujU2(&t), bo 3.58.23 usuwa parametr wariant.
  rm -rf "$ROB/t22up"; mkdir -p "$ROB/t22up/v3.58.22-dht22-log/tests"; cp -r "$T22/." "$ROB/t22up/v3.58.22-dht22-log/tests/"
  python3 - "$ROB/t22up/v3.58.22-dht22-log/tests/test_zew2_filtr.cpp" <<'PY'
import sys, re
p = sys.argv[1]; s = open(p, encoding='utf-8').read()
s2, ile = re.subn(r'zew2DekodujU2\(&(\w+), &\w+\)', r'zew2DekodujU2(&\1)', s)
assert ile == 3, ile
open(p, 'w', encoding='utf-8').write(s2)
PY
  W=$(CPP_PATH="$ROB/jako_35822.cpp" "$ROB/t22up/v3.58.22-dht22-log/tests/uruchom_testy.sh" 2>&1 || true)
  echo "$W" | grep -E "WSZYSTKO OK|BLEDOW|BLAD|sprawdzen" | sed 's/^/    /'
  # Dopuszczalne niezaliczenia = wylacznie skutki celowych zmian 3.58.23:
  #  - scenariusze "BLAD [plik:linia] opis": wariant 16/12, dawne 12-bit (+409.5/+408.8/+404.4/+369.6, ESPHome),
  #    WARN o klonie U2, licznik u2=6, rytm "co 10." (linia przy probie podzielnej przez 10);
  #  - znaczniki "BLAD: ...": stale 30/co 10., warunek "co 10.", dekoder z wariantem, naglowek 3.58.22.
  NB=$(echo "$W" | grep -c "BLAD" || true)
  NDOP=$(echo "$W" | grep -cE "BLAD \[[^]]*\] (wariant 16 zglaszany|\+409\.5 -> -0\.1 \(U2 12-bit\)|\+408\.8 -> -0\.8|\+404\.4 -> -5\.2|\+369\.6 -> -40\.0|U2 12-bit: kazda z 400|\+408\.8 -> -0\.8, RH 91\.5|\+407\.9 -> -1\.7|licznik u2=6|WARN o klonie U2|10 zwyklych ramek po 30\. probie|tresc linii: proba=<wielokrotnosc 10>|powrot do ok w < 10 min)|BLAD: (stale: pierwsze 30 prob|warunek: pierwsze 30|filtr, odniesienie DS i dekoder z 3\.58\.20/21 bez zmian|naglowek i historia zmian 3\.58\.22)" || true)
  if echo "$W" | grep -q "TELEMETRIA-DHT: .* 0 bledow" && [ "$NB" = "$NDOP" ] && [ "$NB" -gt 0 ]; then
    echo "    [OK] testy 3.58.22 (A/B/B2/C) na tym pliku: $NB niezaliczen i wszystkie sa skutkiem celowych zmian 3.58.23 (12-bit, wariant, WARN u2, rytm co 10.)"
  else echo "    BLAD: testy 3.58.22 (niezaliczen $NB, dopuszczalnych $NDOP)"; BLEDY=$((BLEDY+1)); fi
fi

echo; if [ "$BLEDY" = 0 ]; then echo "WSZYSTKO OK"; else echo "BLEDOW: $BLEDY"; exit 1; fi

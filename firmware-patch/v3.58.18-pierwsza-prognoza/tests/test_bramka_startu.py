#!/usr/bin/env python3
"""3.58.18: pierwsze pobranie z Open-Meteo ~60 s (DESZCZ) / ~120 s (PROGNOZA) po starcie.

Wycina z main_centrala.cpp PRAWDZIWE definicje stalych i licznikow (OW_*_FETCH_INTERVAL_MS,
OW_*_PIERWSZE_PO_MS, owDeszczLastFetchMillis, owPrognozaLastFetchMillis) oraz prawdziwy
warunek bramki z zadania TG i kompiluje g++ dwa razy: z `unsigned long` 32-bitowym jak na
ESP32 (uint32_t) i natywnie 64-bitowym. Symuluje millis() od 0 i sprawdza, kiedy bramka
puszcza pierwsze i kolejne pobrania, oraz ze przepelnienie millis() (49,7 dnia) nic nie psuje.
Uzycie: test_bramka_startu.py <main_centrala.cpp>
"""
import re, subprocess, sys, tempfile, os

cpp = sys.argv[1]
s = open(cpp, encoding='utf-8').read()
bledy = 0
def spr(w, co):
    global bledy
    print(('    [OK] ' if w else '    BLAD: ') + co)
    if not w: bledy += 1

def linia(wzor):
    m = re.search(wzor, s, re.M)
    if not m: raise SystemExit('brak w pliku: ' + wzor)
    return m.group(0)

defs = [linia(r'^static const unsigned long OW_DESZCZ_FETCH_INTERVAL_MS = .*;$'),
        linia(r'^static const unsigned long OW_DESZCZ_PIERWSZE_PO_MS = .*;$'),
        linia(r'^unsigned long owDeszczLastFetchMillis = .*;$'),
        linia(r'^static const unsigned long OW_PROGNOZA_FETCH_INTERVAL_MS = .*;$'),
        linia(r'^static const unsigned long OW_PROGNOZA_PIERWSZE_PO_MS = .*;$'),
        linia(r'^unsigned long owPrognozaLastFetchMillis = .*;$')]
# prawdziwe warunki bramek (bez WiFi i budzetu - te nie zaleza od czasu)
wD = linia(r'^\s*millis\(\) - owDeszczLastFetchMillis >= OW_DESZCZ_FETCH_INTERVAL_MS &&$').strip()[:-3]
wP = linia(r'^\s*millis\(\) - owPrognozaLastFetchMillis >= OW_PROGNOZA_FETCH_INTERVAL_MS &&$').strip()[:-3]
spr('owDeszczLastFetchMillis = millis();' in s and 'owPrognozaLastFetchMillis = millis();' in s, 'po pobraniu licznik = millis() (jak dotad)')

HARNESS = r'''
#include <cstdio>
#include <cstdint>
#include <cstdlib>
typedef UL_T unsigned_long_t;
#define unsigned_long unsigned_long_t
static unsigned_long_t g_ms = 0;
static unsigned_long_t millis() { return g_ms; }
#define unsigned long unsigned_long_t
DEFS
#undef unsigned
int main(int, char** argv) {
  // argv[1] = start millis (0 albo tuz przed przepelnieniem 32-bit), symulacja co 1 s przez 40 min
  unsigned long long start = strtoull(argv[1], 0, 10);
  const bool poPobraniu = argv[2][0] == '1';   // 1 = tuz po udanym pobraniu (licznik = millis()), 0 = swiezy start
  int nD = 0, nP = 0; long long firstD = -1, firstP = -1, secondD = -1;
  for (long long t = 0; t <= 40LL * 60 * 1000; t += 1000) {
    g_ms = (unsigned_long_t)(start + t);
    if (t == 0 && poPobraniu) { owDeszczLastFetchMillis = millis(); owPrognozaLastFetchMillis = millis(); }
    if (COND_D) { owDeszczLastFetchMillis = millis(); nD++; if (firstD < 0) firstD = t; else if (secondD < 0) secondD = t; }
    if (COND_P) { owPrognozaLastFetchMillis = millis(); nP++; if (firstP < 0) firstP = t; }
  }
  printf("%lld %lld %lld %d %d\n", firstD, secondD, firstP, nD, nP);
  return 0;
}
'''
for nazwa, typ in (('ESP32 (unsigned long = 32 bity)', 'uint32_t'), ('PC 64-bit', 'unsigned long long')):
    kod = HARNESS.replace('UL_T', typ).replace('DEFS', '\n'.join(defs)).replace('COND_D', wD).replace('COND_P', wP)
    # 'static const unsigned long X' -> po makrze 'unsigned' znika slowo, zostaje 'long' -> podmieniamy typ jawnie
    kod = kod.replace('#define unsigned long unsigned_long_t\n', '').replace('#undef unsigned\n', '')
    kod = kod.replace('static const unsigned long ', 'static const unsigned_long_t ').replace('\nunsigned long ow', '\nunsigned_long_t ow')
    # na ESP32 0UL jest 32-bitowe jak zmienna; na PC 0UL to 64 bity, wiec do emulacji rzutujemy jawnie
    kod = kod.replace('0UL - (', '(unsigned_long_t)0 - (')
    d = tempfile.mkdtemp()
    src = os.path.join(d, 'h.cpp'); open(src, 'w').write(kod)
    exe = os.path.join(d, 'h')
    r = subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-o', exe, src], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr); spr(False, nazwa + ': kompilacja'); continue
    spr(True, nazwa + ': kompilacja bez ostrzezen (-Wall -Wextra -Werror)')
    # 1) swiezy start (millis()=0): pierwsze pobrania 60 s / 120 s, potem co 15 min
    w = subprocess.run([exe, '0', '0'], capture_output=True, text=True).stdout.split()
    firstD, secondD, firstP, nD, nP = map(int, w)
    print('        start millis()=0: DESZCZ pierwsze %d s, drugie %d s, razem %d w 40 min; PROGNOZA pierwsze %d s, razem %d' % (firstD // 1000, secondD // 1000, nD, firstP // 1000, nP))
    spr(firstD == 60 * 1000, nazwa + ': DESZCZ pierwszy raz dokladnie 60 s po starcie')
    spr(secondD == 60 * 1000 + 15 * 60 * 1000, nazwa + ': DESZCZ drugi raz 15 min pozniej (interwal bez zmian)')
    spr(firstP == 120 * 1000, nazwa + ': PROGNOZA pierwszy raz dokladnie 120 s po starcie')
    spr(nD == 3 and nP == 3, nazwa + ': w 40 min po 3 pobrania kazdego (60 s / 15 min / 30 min)')
    # 2) praca ciagla przez przepelnienie millis() (49,7 dnia): pobranie 5 min przed zawinieciem,
    #    nastepne ma byc dokladnie 15 min pozniej (10 min PO zawinieciu) - jak dotad
    if typ == 'uint32_t':
        w = subprocess.run([exe, str((1 << 32) - 5 * 60 * 1000), '1'], capture_output=True, text=True).stdout.split()
        firstD, secondD, firstP, nD, nP = map(int, w)
        print('        pobranie 5 min przed przepelnieniem: nastepne DESZCZ po %d s, PROGNOZA po %d s, razem %d / %d w 40 min' % (firstD // 1000, firstP // 1000, nD, nP))
        spr(firstD == 15 * 60 * 1000 and firstP == 15 * 60 * 1000, nazwa + ': przepelnienie millis() - kolejne pobranie dokladnie 15 min po poprzednim')
        spr(nD == 2 and nP == 2, nazwa + ': przez przepelnienie bez dodatkowych ani zgubionych pobran')

print(('ZALICZONY: bramka startu OK' if bledy == 0 else 'BLEDOW: %d' % bledy))
sys.exit(1 if bledy else 0)

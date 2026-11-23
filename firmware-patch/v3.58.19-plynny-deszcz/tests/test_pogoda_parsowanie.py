#!/usr/bin/env python3
"""Kompiluje na PC (g++) prawdziwy kod parsowania odpowiedzi Open-Meteo z main_centrala.cpp
(pomocnicze funkcje + cialo pobierzPrognozeDeszczu() za http.getString()) z atrapa String
i sprawdza deszcz_stan/deszcz_min (jak dotad) oraz nowe pogoda_kod / pogoda_kod_opad /
pogoda_dzien / wiatr_porywy na 5 odpowiedziach (snieg za 2 h, pada w nocy, bezchmurnie,
odpowiedz bez nowych pol, null w slocie opadu) oraz [3.58.17] pogoda_temp (ujemna, dodatnia, 0.0,
nieobecna, null) oraz [3.58.19] plynne odliczanie: przesuniecie w slocie z zegara centrali, chwile
poczatku/konca, deszczStanTeraz() po uplywie czasu (schodzenie co minute, przejscie w "pada" i w stan 0,
pobranie 15 min pozniej bez skoku, zegar niezgodny ze slotem, przepelnienie millis()). Uzycie: python3 test_pogoda_parsowanie.py [sciezka.cpp]"""
import os, subprocess, sys, tempfile
cpp = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'src', 'main_centrala.cpp')
s = open(cpp, encoding='utf-8').read()
a = s.index('int deszczStan = 0;'); b = s.index('void pobierzPrognozeDeszczu() {')
f0 = s.index('  // "current":{ jest jednoznaczne', b); f1 = s.index('\n}\n', f0)
h = open(os.path.join(os.path.dirname(__file__), 'harness_pogoda.cpp.in'), encoding='utf-8').read()
h = h.replace('//@@POM@@', s[a:b]).replace('//@@CIALO@@', s[f0:f1])
d = tempfile.mkdtemp(prefix='pogoda_')
kod = 0
# dwie budowy: natywna 64-bit i "jak ESP32" (unsigned long = 32 bity, long = 32 bity) - ta druga
# sprawdza naprawde arytmetyke przepelnienia millis() w deszczStanTeraz()
for nazwa, tekst, flagi in (('64-bit', h, []),
                            ('32-bit jak ESP32', h.replace('unsigned long', 'uint32_t').replace('(long)(', '(int32_t)(').replace('const long do', 'const int32_t do').replace('long poMin, long poSek', 'int32_t poMin, int32_t poSek'), ['-DSIZEOF_UL="4"'])):
    print('    -- budowa %s --' % nazwa)
    src = os.path.join(d, 'test_pogoda.cpp'); open(src, 'w', encoding='utf-8').write('#include <cstdint>\n' + tekst)
    r = subprocess.run(['g++', '-std=c++17', '-O1', '-Wall'] + flagi + ['-o', os.path.join(d, 'test_pogoda'), src], capture_output=True, text=True)
    if r.returncode != 0:
        print('    BLAD: kompilacja harnessu:\n' + r.stderr[-3000:]); sys.exit(1)
    r = subprocess.run([os.path.join(d, 'test_pogoda')], capture_output=True, text=True)
    print(r.stdout, end=''); kod = kod or r.returncode
sys.exit(kod)

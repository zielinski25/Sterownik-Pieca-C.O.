#!/usr/bin/env python3
"""Kompiluje na PC (g++) prawdziwy kod parsowania odpowiedzi Open-Meteo z main_centrala.cpp
(pomocnicze funkcje + cialo pobierzPrognozeDeszczu() za http.getString()) z atrapa String
i sprawdza deszcz_stan/deszcz_min (jak dotad) oraz nowe pogoda_kod / pogoda_kod_opad /
pogoda_dzien / wiatr_porywy na 5 odpowiedziach (snieg za 2 h, pada w nocy, bezchmurnie,
odpowiedz bez nowych pol, null w slocie opadu) oraz [3.58.17] pogoda_temp (ujemna, dodatnia, 0.0,
nieobecna, null). Uzycie: python3 test_pogoda_parsowanie.py [sciezka.cpp]"""
import os, subprocess, sys, tempfile
cpp = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'src', 'main_centrala.cpp')
s = open(cpp, encoding='utf-8').read()
a = s.index('int deszczStan = 0;'); b = s.index('void pobierzPrognozeDeszczu() {')
f0 = s.index('  // "current":{ jest jednoznaczne', b); f1 = s.index('\n}\n', f0)
h = open(os.path.join(os.path.dirname(__file__), 'harness_pogoda.cpp.in'), encoding='utf-8').read()
h = h.replace('//@@POM@@', s[a:b]).replace('//@@CIALO@@', s[f0:f1])
d = tempfile.mkdtemp(prefix='pogoda_')
src = os.path.join(d, 'test_pogoda.cpp'); open(src, 'w', encoding='utf-8').write(h)
r = subprocess.run(['g++', '-std=c++17', '-O1', '-o', os.path.join(d, 'test_pogoda'), src], capture_output=True, text=True)
if r.returncode != 0:
    print('    BLAD: kompilacja harnessu:\n' + r.stderr[-3000:]); sys.exit(1)
r = subprocess.run([os.path.join(d, 'test_pogoda')], capture_output=True, text=True)
print(r.stdout, end=''); sys.exit(r.returncode)

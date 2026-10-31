#!/usr/bin/env python3
"""214: uruchom brame i jej selftest, zapisz wynik.

Dwa etapy, kazdy z wlasnym kodem wyjscia:
  1. build.py   - patche, odwracalnosc, 18 kontroli (S01..S18),
  2. selftest.py - 16 przypadkow dowodzacych, ze brama lapie zepsuty plik.

Wynik: WYNIKI/runNNN/wynik.json (zostaje nawet przy bledzie).
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

P = Path(__file__).resolve().parent
R = P.parents[1]
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'
NARZ = R / 'narzedzia'

inputs = [P / n for n in ('zrob_patch.py', 'build.py', 'sprawdz.py', 'selftest.py',
                          'main_centrala-v3.56.1-to-v3.56.2.patch',
                          'main_centrala-v3.55.8-to-v3.56.2.patch')]
inputs += [NARZ / 'wysylka-zapinka211' / 'HistoriaPin.h',
           NARZ / 'panel-pobrania213' / 'HistoriaPobranie.h',
           NARZ / 'panel-pobrania213' / 'HistoriaBilety.h',
           NARZ / 'panel-pobrania213' / 'host' / 'Serwer213.h',
           NARZ / 'wysylka-zapinka211' / 'host' / 'Arduino.h',
           FW / 'main_centrala.cpp', FW / 'secrets.h.example']

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

wynik = {
    'status': 'RUNNING',
    'stage': 214,
    'release_ready': False,
    'main_changed': False,
    'device_touched': False,
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'kroki': {},
}


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2) + '\n')


KROKI = [
    ('brama', ['build.py'], 'PASS brama214'),
    ('selftest', ['selftest.py'], 'PASS selftest214'),
]

try:
    for nazwa, cmd, oczekiwane in KROKI:
        t = subprocess.run([sys.executable, '-B', str(P / cmd[0])], cwd=str(P),
                           capture_output=True, text=True, timeout=1800)
        wynik['kroki'][nazwa] = {'returncode': t.returncode, 'stdout': t.stdout[-8000:],
                                 'stderr': t.stderr[-4000:]}
        zapisz()
        assert t.returncode == 0, (nazwa, t.stdout[-3000:], t.stderr[-2000:])
        linie = [l for l in t.stdout.split('\n') if l.startswith(oczekiwane)]
        assert linie, (nazwa, t.stdout[-3000:])
        print(linie[-1])  # ostatnia linia z markerem = podsumowanie kroku
    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
print('PASS run214 krokow=%d raport=%s' % (len(KROKI), out.relative_to(R)))
sys.exit(0)

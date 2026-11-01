#!/usr/bin/env python3
"""215: uruchom brame API i zapisz wynik.

Brama kompiluje nasze naglowki wobec PRAWDZIWEGO ESPAsyncWebServer.h
(przypiety w repo, SHA-256) w dwoch standardach C++ i z dwiema kontrolami
negatywnymi. Wynik: WYNIKI/runNNN/wynik.json.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

P = Path(__file__).resolve().parent
R = P.parents[1]
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'
N = R / 'narzedzia'

inputs = [P / n for n in ('kompiluj.py', 'przygotuj_api.py', 'Extracted215.inc',
                          'shim/Arduino.h', 'shim/WString.h', 'shim/sdkconfig.h')]
inputs += [N / 'uchwyty-runtime197' / 'upstream' / 'ESPAsyncWebServer.h',
           N / 'odpowiedz-http199' / 'upstream' / 'WebResponseImpl.h',
           N / 'naglowki-upload206' / 'upstream' / 'literals.h',
           N / 'naglowki-upload206' / 'upstream' / 'WebHandlerImpl.h',
           N / 'blokada-startu-fs196' / 'upstream' / 'FS.h',
           N / 'wysylka-zapinka211' / 'HistoriaPin.h',
           N / 'panel-pobrania213' / 'HistoriaPobranie.h',
           N / 'panel-pobrania213' / 'HistoriaBilety.h',
           N / 'brama-przed-wgraniem214' / 'main_centrala-v3.55.8-to-v3.56.2.patch',
           FW / 'main_centrala.cpp', FW / 'CheckedHistoryIO.h']

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

wynik = {
    'status': 'RUNNING',
    'stage': 215,
    'release_ready': False,
    'main_changed': False,
    'device_touched': False,
    'compiled_against': 'real ESPAsyncWebServer.h (mathieucarbou fork, pinned)',
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'checks': {},
}


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2) + '\n')


try:
    t = subprocess.run([sys.executable, '-B', str(P / 'kompiluj.py')], cwd=str(P),
                       capture_output=True, text=True, timeout=1800)
    wynik['checks']['api'] = {'returncode': t.returncode, 'stdout': t.stdout[-8000:],
                              'stderr': t.stderr[-4000:]}
    zapisz()
    assert t.returncode == 0, (t.stdout[-3000:], t.stderr[-2000:])
    linie = [l for l in t.stdout.split('\n') if l.startswith('PASS api215 sprawdzen=')]
    assert linie, t.stdout[-2000:]
    print(linie[-1])
    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
print('PASS run215 raport=%s' % out.relative_to(R))
sys.exit(0)

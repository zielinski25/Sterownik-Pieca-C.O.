#!/usr/bin/env python3
"""213: testy w trzech profilach + REGRESJA etapu 212.

Dwa zestawy w kazdym profilu:
  * panel213 - nowy przycisk, bilety, trasa v3.56.1,
  * trasa212 - CALY zestaw etapu 212 (20 grup / 132 sprawdzenia) skompilowany
    przeciwko NOWEMU naglowkowi HistoriaPobranie.h. Jesli 213 zepsulaby trase
    212, ten zestaw to pokaze.

Wynik: WYNIKI/runNNN/wynik.json.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile

P = Path(__file__).resolve().parent
R = P.parents[1]
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'
S211 = R / 'narzedzia' / 'wysylka-zapinka211'
S212 = R / 'narzedzia' / 'pobranie-trasa212'

subprocess.run([sys.executable, '-B', str(P / 'build.py')], check=True)

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

inputs = [P / n for n in ('Test.cpp', 'HistoriaBilety.h', 'HistoriaPobranie.h',
                          'zrob_patch.py', 'build.py', 'host/Model213.h', 'host/Serwer213.h',
                          'main_centrala-v3.56.0-to-v3.56.1.patch',
                          'main_centrala-v3.55.8-to-v3.56.1.patch')]
inputs += [S211 / n for n in ('HistoriaPin.h', 'host/Arduino.h', 'host/RamCalls.h')]
inputs += [S212 / n for n in ('Test.cpp', 'Extracted212.inc', 'HistoriaPobranie.h')]
inputs += [FW / 'CheckedHistoryIO.h', FW / 'main_centrala.cpp']

wynik = {
    'status': 'RUNNING',
    'stage': 213,
    'release_ready': False,
    'main_changed': False,
    'device_touched': False,
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'profiles': {},
}


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2) + '\n')


PROFILE = [
    ('native', ['-O2', '-Werror', '-fno-exceptions', '-fno-rtti'], {}),
    ('asan-ubsan', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer', '-fno-exceptions', '-fno-rtti'],
     {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'}),
    ('glibcxx-assertions', ['-O2', '-Werror', '-D_GLIBCXX_ASSERTIONS',
                            '-fno-exceptions', '-fno-rtti'], {}),
]

INCLUDE = ['-I' + str(P), '-I' + str(P / 'host'), '-I' + str(S211), '-I' + str(S211 / 'host'),
           '-I' + str(S212), '-I' + str(S212 / 'host'), '-I' + str(FW)]


def przygotuj_regresje(katalog):
    """Kopia Test.cpp z 212, ktora bierze NOWY naglowek trasy i nowszy model serwera.

    Zmieniamy wylacznie sciezki i nazwe modelu - zadnej linii testu.
    """
    t = (S212 / 'Test.cpp').read_text(encoding='utf-8')
    t = t.replace('../wysylka-zapinka211/', str(S211) + '/')
    t = t.replace('#include "Serwer212.h"',
                  '#include "Serwer213.h"   // model 212 + parametry zapytania')
    cel = Path(katalog) / 'Test212Regresja.cpp'
    cel.write_text(t, encoding='utf-8')
    return cel


try:
    with tempfile.TemporaryDirectory() as td:
        regresja = przygotuj_regresje(td)
        for label, flags, env in PROFILE:
            for nazwa, zrodlo, oczekiwane in (
                    ('panel213', str(P / 'Test.cpp'), 'PASS panel213'),
                    ('trasa212-regresja', str(regresja), 'PASS trasa212')):
                exe = Path(td) / ('test-%s-%s' % (label, nazwa))
                cmd = ['c++', '-std=c++17', '-Wall', '-Wextra', '-pthread', *flags,
                       *INCLUDE, zrodlo, '-o', str(exe)]
                budowa = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
                assert budowa.returncode == 0, (label, nazwa, budowa.stderr[-4000:])
                uruchom = subprocess.run([str(exe)], capture_output=True, text=True, timeout=600,
                                         env=dict(os.environ, **env))
                wpis = wynik['profiles'].setdefault(label, {})
                wpis[nazwa] = {
                    'compile': 'ok',
                    'returncode': uruchom.returncode,
                    'stdout': uruchom.stdout,
                    'stderr_tail': uruchom.stderr[-4000:],
                }
                zapisz()
                assert uruchom.returncode == 0, (label, nazwa, uruchom.stdout[-2000:],
                                                 uruchom.stderr[-2000:])
                podsumowanie = [l for l in uruchom.stdout.split('\n') if l.startswith(oczekiwane)]
                assert len(podsumowanie) == 1, (label, nazwa, uruchom.stdout)
                print(podsumowanie[0] + '  [' + label + ', ' + nazwa + ']')
    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
print('PASS run213 profiles=%d zestawy=2 raport=%s' % (len(PROFILE), out.relative_to(R)))
sys.exit(0)

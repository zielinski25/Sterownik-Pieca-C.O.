#!/usr/bin/env python3
"""217: brama wytrzymalosci - setki pobran i wysylek + kontrole negatywne.

Kroki:
  1. build.py  - plik po patchu v3.56.3 (SHA z etapu 216) i wyciagniecie
                 PRAWDZIWEGO kodu: oba wysylacze Telegram, trasa, zapinka,
  2. Test.cpp  - 9 grup: 300 pobran HTTP, 50 zerwanych, dwa naraz, blad VFS,
                 zajety zapis, 200 wysylek Telegram, 5 sciezek awarii,
                 hist_long, bilans uchwytow - w trzech profilach,
  3. mutanty.py - M1/M2/M3: celowo zepsute zwalnianie uchwytu i czyszczenie
                 flagi MUSZA wywalic test.

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
N = R / 'narzedzia'
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'
D211 = N / 'wysylka-zapinka211'
D212 = N / 'pobranie-trasa212'
D213 = N / 'panel-pobrania213'

INCLUDE = ['-I' + str(P), '-I' + str(D213), '-I' + str(D213 / 'host'),
           '-I' + str(D211), '-I' + str(D211 / 'host'),
           '-I' + str(D212), '-I' + str(D212 / 'host'), '-I' + str(FW)]

PROFILE = [
    ('native', ['-O2', '-Werror', '-fno-exceptions', '-fno-rtti'], {}),
    ('asan-ubsan', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer', '-fno-exceptions', '-fno-rtti'],
     {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'}),
    ('glibcxx-assertions', ['-O2', '-Werror', '-D_GLIBCXX_ASSERTIONS',
                            '-fno-exceptions', '-fno-rtti'], {}),
]

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

inputs = [P / n for n in ('build.py', 'mutanty.py', 'Test.cpp')]
inputs += [D213 / 'HistoriaPobranie.h', D213 / 'HistoriaBilety.h',
           D211 / 'HistoriaPin.h', D211 / 'Extracted211.inc',
           N / 'od-przycisku216' / 'main_centrala-v3.55.8-to-v3.56.3.patch',
           FW / 'main_centrala.cpp']

wynik = {
    'status': 'RUNNING',
    'stage': 217,
    'release_ready': False,
    'main_changed': False,
    'firmware_version': 'v3.56.3 (bez zmian w tym etapie)',
    'device_touched': False,
    'inputs': {},
    'profiles': {},
    'mutanty': {},
}
for p in inputs:
    if p.exists():
        wynik['inputs'][str(p.relative_to(R))] = hashlib.sha256(p.read_bytes()).hexdigest()


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2) + '\n')


def krok(nazwa, cmd, oczekiwany=0, env=None):
    t = subprocess.run(cmd, capture_output=True, text=True, timeout=3600,
                       env=dict(os.environ, **(env or {})))
    wynik.setdefault('kroki', []).append({'nazwa': nazwa, 'cmd': cmd,
                                          'returncode': t.returncode,
                                          'stdout': t.stdout[-20000:],
                                          'stderr_tail': t.stderr[-8000:]})
    zapisz()
    if oczekiwany is not None and t.returncode != oczekiwany:
        raise SystemExit('krok %s zwrocil %d\n%s\n%s'
                         % (nazwa, t.returncode, t.stdout[-3000:], t.stderr[-3000:]))
    return t


def main():
    print(krok('build', [sys.executable, '-B', str(P / 'build.py')]).stdout.strip())

    exeDir = tempfile.mkdtemp(prefix='bin217-')
    for label, flags, env in PROFILE:
        exe = Path(exeDir) / ('test-%s' % label)
        cmd = ['c++', '-std=c++17', '-Wall', '-Wextra', '-pthread', *flags,
               *INCLUDE, str(P / 'Test.cpp'), '-o', str(exe)]
        budowa = subprocess.run(cmd, capture_output=True, text=True, timeout=1800)
        wynik['profiles'].setdefault(label, {})['compile'] = budowa.returncode
        zapisz()
        if budowa.returncode != 0:
            raise SystemExit('kompilacja [%s]:\n%s' % (label, budowa.stderr[-4000:]))
        t = krok('wytrzymalosc217 [%s]' % label, [str(exe)], env=env)
        wynik['profiles'][label]['run'] = t.returncode
        wynik['profiles'][label]['podsumowanie'] = \
            [l for l in t.stdout.split('\n') if l.startswith('PASS wytrzymalosc217')][-1]
        print(t.stdout.strip().split('\n')[-1] + '  [' + label + ']')

    t = krok('mutanty', [sys.executable, '-B', str(P / 'mutanty.py')])
    wynik['mutanty'] = {'returncode': t.returncode,
                        'linie': [l for l in t.stdout.split('\n') if l.startswith('PASS mutant M')]}
    print(t.stdout.strip().split('\n')[-1])

    wynik['status'] = 'PASS'
    zapisz()
    print('PASS brama217 profili=%d mutantow=%d sprawdzen_w_grupach=66'
          % (len(wynik['profiles']), len(wynik['mutanty']['linie'])))


if __name__ == '__main__':
    try:
        main()
    except Exception as blad:  # noqa: BLE001
        wynik['status'] = 'FAIL'
        wynik['error'] = str(blad)
        zapisz()
        raise

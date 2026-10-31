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

subprocess.run(['python3', '-B', str(P / 'build.py')], check=True)

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

inputs = [P / n for n in ('Test.cpp', 'HistoriaPin.h', 'Extracted211.inc',
                          'main_centrala-v3.55.8-to-v3.55.9.patch', 'build.py',
                          'host/Arduino.h', 'host/RamCalls.h', 'host/Firmware211.h',
                          'NowaKrotka.inc', 'NowaDluga.inc')]

wynik = {
    'status': 'RUNNING',
    'stage': 211,
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
                    '-fno-omit-frame-pointer', '-no-pie', '-fno-exceptions', '-fno-rtti'],
     {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'}),
    ('glibcxx-assertions', ['-O2', '-Werror', '-D_GLIBCXX_ASSERTIONS', '-fno-exceptions', '-fno-rtti'], {}),
]

try:
    with tempfile.TemporaryDirectory() as td:
        for label, flags, env in PROFILE:
            exe = Path(td) / ('test-' + label)
            cmd = ['c++', '-std=c++17', '-Wall', '-Wextra', '-pthread',
                   *flags, '-I' + str(P), '-I' + str(P / 'host'), '-I' + str(FW),
                   str(P / 'Test.cpp'), '-o', str(exe)]
            budowa = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
            assert budowa.returncode == 0, (label, budowa.stderr[-4000:])
            uruchom = subprocess.run([str(exe)], capture_output=True, text=True, timeout=600,
                                     env=dict(os.environ, **env))
            wynik['profiles'][label] = {
                'compile': 'ok',
                'returncode': uruchom.returncode,
                'stdout': uruchom.stdout,
                'stderr_tail': uruchom.stderr[-4000:],
            }
            zapisz()
            assert uruchom.returncode == 0, (label, uruchom.stdout[-2000:], uruchom.stderr[-2000:])
            podsumowanie = [l for l in uruchom.stdout.split('\n') if l.startswith('PASS route211')]
            assert len(podsumowanie) == 1, (label, uruchom.stdout)
            print(podsumowanie[0] + '  [' + label + ']')
    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
print('PASS run211 profiles=%d raport=%s' % (len(PROFILE), out.relative_to(R)))
sys.exit(0)

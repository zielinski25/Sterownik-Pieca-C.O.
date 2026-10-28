#!/usr/bin/env python3
"""218: brama "panel -> tabela tras -> plik". Trzy profile + mutacje.

Kolejnosc (wszystko od zera, nic nie jest brane "na wiare"):
  1. build.py: v3.55.8 + 211..216 + 218 -> v3.56.4, tabele tras, wyciagi,
  2. panel218.mjs: PRAWDZIWY skrypt panelu w silniku JS -> adresy, ktore panel wola,
  3. Test.cpp: te adresy podane PRAWDZIWEJ tabeli tras (3 profile kompilacji),
  4. mutacje: kazda zepsuta wersja MUSI zrobic brame czerwona.

Wynik: WYNIKI/runNNN/wynik.json.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

P = Path(__file__).resolve().parent
R = P.parents[1]
N = R / 'narzedzia'
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'
S211 = N / 'wysylka-zapinka211'
S212 = N / 'pobranie-trasa212'
S213 = N / 'panel-pobrania213'
S216 = N / 'od-przycisku216'

subprocess.run([sys.executable, '-B', str(P / 'build.py')], check=True)

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

INCLUDE = ['-I' + str(P), '-I' + str(P / 'host'), '-I' + str(S216), '-I' + str(S213),
           '-I' + str(S211), '-I' + str(S211 / 'host'), '-I' + str(S212),
           '-I' + str(S212 / 'host'), '-I' + str(FW)]

inputs = [P / n for n in ('Test.cpp', 'build.py', 'zrob_patch.py', 'trasy.py', 'panel218.mjs',
                          'Extracted218.inc', 'Panel218.html', 'Trasy218.json',
                          'host/Serwer218.h',
                          'main_centrala-v3.56.3-to-v3.56.4.patch',
                          'main_centrala-v3.55.8-to-v3.56.4.patch')]
inputs += [S216 / 'HistoriaPobranie.h', S213 / 'HistoriaBilety.h',
           S211 / 'HistoriaPin.h', S211 / 'host' / 'Arduino.h', S211 / 'host' / 'RamCalls.h',
           S212 / 'Extracted212.inc', FW / 'main_centrala.cpp']

wynik = {
    'status': 'RUNNING',
    'stage': 218,
    'release_ready': False,
    'main_changed': True,        # TEN etap zmienia main_centrala.cpp (patch v3.56.4)
    'device_touched': False,
    'firmware': {'przed': 'v3.56.3', 'po': 'v3.56.4'},
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'profiles': {},
    'mutacje': {},
}


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2, ensure_ascii=False) + '\n')


PROFILE = [
    ('native', ['-O2', '-Werror', '-fno-exceptions', '-fno-rtti'], {}),
    ('asan-ubsan', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer', '-fno-exceptions', '-fno-rtti'],
     {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'}),
    ('glibcxx-assertions', ['-O2', '-Werror', '-D_GLIBCXX_ASSERTIONS',
                            '-fno-exceptions', '-fno-rtti'], {}),
]


def zbuduj_i_uruchom(zrodlo, exe, flagi, env, dodatkowe_include=()):
    cmd = ['c++', '-std=c++17', '-Wall', '-Wextra', '-pthread', *flagi,
           *dodatkowe_include, *INCLUDE, str(zrodlo), '-o', str(exe)]
    budowa = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    if budowa.returncode != 0:
        return {'compile': 'BLAD', 'stderr_tail': budowa.stderr[-4000:]}
    uruchom = subprocess.run([str(exe), str(out / 'urls.tsv')], capture_output=True, text=True,
                             timeout=900, env=dict(os.environ, **env))
    return {'compile': 'ok', 'returncode': uruchom.returncode,
            'stdout': uruchom.stdout, 'stderr_tail': uruchom.stderr[-4000:]}


def uruchom_panel(plik_html, plik_json):
    t = subprocess.run(['node', str(P / 'panel218.mjs'), str(plik_html), str(plik_json)],
                       capture_output=True, text=True, timeout=300)
    return t


def tsv_z_json(plik_json, plik_tsv):
    with open(plik_json, 'r', encoding='utf-8') as f:
        dane = json.load(f)
    linie = []
    for s in dane['sekwencje']:
        linie.append('%s\t%s\t%s' % (s['nazwa'], s['bilet_url'], s['pobranie_url']))
    Path(plik_tsv).write_text('\n'.join(linie) + '\n', encoding='utf-8')
    return linie


def katalog_mutanta(td, nazwa, podmiany_inc=(), podmiany_html=()):
    """Katalog z Test.cpp + podmienionym Extracted218.inc / Panel218.html."""
    d = Path(td) / nazwa
    (d / 'x').mkdir(parents=True)
    (d / 'pobranie-trasa212').mkdir()
    shutil.copyfile(P / 'Test.cpp', d / 'x' / 'Test.cpp')
    shutil.copyfile(S212 / 'Extracted212.inc', d / 'pobranie-trasa212' / 'Extracted212.inc')
    inc = (P / 'Extracted218.inc').read_text(encoding='utf-8')
    for stary, nowy in podmiany_inc:
        if inc.count(stary) != 1:
            raise SystemExit('mutant %s: oczekiwano jednego "%s"' % (nazwa, stary[:40]))
        inc = inc.replace(stary, nowy, 1)
    (d / 'x' / 'Extracted218.inc').write_text(inc, encoding='utf-8')
    html = (P / 'Panel218.html').read_text(encoding='utf-8')
    for stary, nowy in podmiany_html:
        if html.count(stary) != 1:
            raise SystemExit('mutant %s: oczekiwano jednego "%s"' % (nazwa, stary[:40]))
        html = html.replace(stary, nowy, 1)
    (d / 'x' / 'Panel218.html').write_text(html, encoding='utf-8')
    return d


try:
    # ---- 2. panel w silniku JS -----------------------------------------
    plik_urls = out / 'urls.json'
    panel = uruchom_panel(P / 'Panel218.html', plik_urls)
    wynik['panel'] = {'returncode': panel.returncode, 'stdout': panel.stdout,
                      'stderr_tail': panel.stderr[-2000:]}
    zapisz()
    assert panel.returncode == 0, ('panel218', panel.stdout[-2000:], panel.stderr[-2000:])
    podsumowanie = [l for l in panel.stdout.split('\n') if l.startswith('PASS panel218')]
    assert len(podsumowanie) == 1, panel.stdout
    print(podsumowanie[0] + '  [node]')
    tsv_z_json(plik_urls, out / 'urls.tsv')

    # ---- 3. tabela tras + trzy profile ----------------------------------
    with tempfile.TemporaryDirectory() as td:
        exe = Path(td) / 'trasa218'
        for label, flagi, env in PROFILE:
            wpis = zbuduj_i_uruchom(P / 'Test.cpp', exe, flagi, env)
            wynik['profiles'][label] = wpis
            zapisz()
            assert wpis.get('compile') == 'ok', (label, wpis.get('stderr_tail'))
            assert wpis['returncode'] == 0, (label, wpis['stdout'][-3000:])
            podsumowanie = [l for l in wpis['stdout'].split('\n') if l.startswith('PASS trasa218')]
            assert len(podsumowanie) == 1, wpis['stdout']
            print(podsumowanie[0] + '  [' + label + ']')

    # ---- 4. mutacje ------------------------------------------------------
    # Kazdy mutant to konkretny blad, ktory ten etap ma lapac.
    MUTANTY = [
        ('M1_goly_napis_history', 'inc',
         [('AsyncURIMatcher::exact("/history")', '"/history"')], [],
         'trasa /history zarejestrowana gołym napisem (dokladnie blad v3.56.3)'),
        ('M2_goly_napis_long', 'inc',
         [('AsyncURIMatcher::exact("/history/long")', '"/history/long"')], [],
         '/history/long zarejestrowane gołym napisem'),
        ('M3_goly_napis_link', 'inc',
         [('AsyncURIMatcher::exact("/history/link")', '"/history/link"')], [],
         '/history/link zarejestrowane gołym napisem - panel nie dostaje biletu'),
        ('M4_panel_zawsze_krotka', 'html',
         [], [("var trasa = (strumien === 'long') ? '/history/long' : '/history';",
               "var trasa = '/history';")],
         'panel otwiera /history takze dla dlugiej (blad v3.56.2)'),
        ('M5_panel_bez_kontroli_biletu', 'html',
         [], [('if (bilet.length !== 32) throw new Error', 'if (false) throw new Error')],
         'panel nie sprawdza dlugosci biletu'),
    ]
    with tempfile.TemporaryDirectory() as td:
        exe = Path(td) / 'trasa218-mutant'
        for nazwa, rodzaj, podmiany_inc, podmiany_html, opis in MUTANTY:
            d = katalog_mutanta(td, nazwa, podmiany_inc, podmiany_html)
            wpis = {'opis': opis}
            if rodzaj == 'inc':
                r = zbuduj_i_uruchom(d / 'x' / 'Test.cpp', exe,
                                     PROFILE[0][1], PROFILE[0][2], ['-I' + str(d / 'x')])
                wpis['returncode'] = r.get('returncode', -1)
                wpis['stdout_tail'] = r.get('stdout', '')[-2000:]
                wpis['wykryty'] = r.get('compile') == 'ok' and r.get('returncode', 0) != 0
            else:
                # Panel: podmieniony HTML musi wywrocic panel218.mjs, a jego adresy
                # (jesli jakies wyjda) musza wywrocic test C++.
                pm = uruchom_panel(d / 'x' / 'Panel218.html', d / 'urls.json')
                panel_czerwony = pm.returncode != 0
                cpp_czerwony = False
                if (d / 'urls.json').exists():
                    tsv_z_json(d / 'urls.json', d / 'urls.tsv')
                    rc = subprocess.run([str(exe), str(d / 'urls.tsv')], capture_output=True,
                                        text=True, timeout=900)
                    cpp_czerwony = rc.returncode != 0
                    wpis['cpp_stdout_tail'] = rc.stdout[-2000:]
                wpis['panel_returncode'] = pm.returncode
                wpis['panel_stdout_tail'] = pm.stdout[-2000:]
                wpis['wykryty'] = panel_czerwony or cpp_czerwony
            wynik['mutacje'][nazwa] = wpis
            zapisz()
            if not wpis['wykryty']:
                raise SystemExit('mutant %s NIE zostal wykryty - brama jest slepa' % nazwa)
            print('PASS mutant %s wykryty (%s)' % (nazwa, opis))

    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
podsumowanie = [l for l in wynik['profiles']['native']['stdout'].split('\n')
                if l.startswith('PASS trasa218')][0]
print('PASS brama218 profili=%d mutantow=%d %s'
      % (len(wynik['profiles']), len(wynik['mutacje']),
         podsumowanie.replace('PASS trasa218 ', '')))

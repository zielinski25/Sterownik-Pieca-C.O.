#!/usr/bin/env python3
"""216: "od przycisku do pliku" - jedna brama na szew panel <-> trasa.

Kolejnosc (kazdy krok musi przejsc, inaczej brama jest CZERWONA):
  1. build.py    - patch v3.56.3: lancuch == skumulowany, cofa sie do v3.55.8,
                   wyciagniete PRAWDZIWE kawalki pliku (panel, trasy, lambdy),
  2. api216.py   - NOWY naglowek kompiluje sie wobec PRAWDZIWEGO API biblioteki
                   (gnu++11 i gnu++17) + kontrola negatywna,
  3. panel_test  - PRAWDZIWY skrypt panelu w silniku JS; wypisuje adresy, pod
                   ktore panel naprawde idzie (urls.txt),
  4. Test.cpp    - te adresy ida do PRAWDZIWEJ trasy z firmware; pobrany plik
                   musi byc wlasciwym plikiem i musi miec nazwe (3 profile),
  5. kontrole negatywne:
       N1  adresy ze STAREGO panelu (v3.56.2) musza wywalic Test.cpp (401),
       N2  STARY naglowek (bez Content-Disposition) musi wywalic Test.cpp,
       N3  literowka w id elementu musi wywalic panel_test,
       N4  zla dlugosc biletu w JS musi wywalic panel_test.

Wynik: WYNIKI/runNNN/wynik.json.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
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

inputs = [P / n for n in ('zrob_patch.py', 'build.py', 'api216.py', 'panel_test.mjs',
                          'Test.cpp', 'main_centrala-v3.56.2-to-v3.56.3.patch',
                          'main_centrala-v3.55.8-to-v3.56.3.patch', 'HistoriaPobranie.h')]
inputs += [D213 / 'HistoriaPobranie.h', D213 / 'HistoriaBilety.h',
           D211 / 'HistoriaPin.h', FW / 'main_centrala.cpp']

wynik = {
    'status': 'RUNNING',
    'stage': 216,
    'release_ready': False,
    'main_changed': False,
    'device_touched': False,
    'inputs': {},
    'profiles': {},
    'kontrole_negatywne': {},
    'kroki': [],
}
for p in inputs:
    if p.exists():
        wynik['inputs'][str(p.relative_to(R))] = hashlib.sha256(p.read_bytes()).hexdigest()


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2) + '\n')


def krok(nazwa, cmd, oczekiwany=0, env=None, cwd=None):
    t = subprocess.run(cmd, capture_output=True, text=True, timeout=1800,
                       env=dict(os.environ, **(env or {})), cwd=cwd)
    wynik['kroki'].append({'nazwa': nazwa, 'cmd': cmd, 'returncode': t.returncode,
                           'stdout': t.stdout[-20000:], 'stderr_tail': t.stderr[-8000:]})
    zapisz()
    if oczekiwany is not None and t.returncode != oczekiwany:
        raise SystemExit('krok %s zwrocil %d (oczekiwano %s)\n%s\n%s'
                         % (nazwa, t.returncode, oczekiwany, t.stdout[-3000:],
                            t.stderr[-3000:]))
    return t


def kompiluj(zrodlo, exe, flags, include=None):
    cmd = ['c++', '-std=c++17', '-Wall', '-Wextra', '-pthread', *flags,
           *(include or INCLUDE), str(zrodlo), '-o', str(exe)]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=1200)


def main():
    # --- 1. patch i ekstrakcja ------------------------------------------
    print(krok('build', [sys.executable, '-B', str(P / 'build.py')]).stdout.strip())
    # --- 2. API ----------------------------------------------------------
    print(krok('api', [sys.executable, '-B', str(P / 'api216.py')]).stdout.strip())
    # --- 3. panel w JS ---------------------------------------------------
    panelOut = out / 'panel'
    panelOut.mkdir()
    t = krok('panel-js', ['node', str(P / 'panel_test.mjs'),
                          str(P / 'panel' / 'panel.html'), str(panelOut)])
    print(t.stdout.strip())
    urls = panelOut / 'urls.txt'

    # --- 4. C++: adresy z panelu -> prawdziwa trasa ----------------------
    # Programy budujemy w katalogu tymczasowym - w paczce maja byc zrodla
    # i wyniki, nie 2 MB skompilowanych plikow.
    exeDir = tempfile.mkdtemp(prefix='bin216-')
    for label, flags, env in PROFILE:
        exe = Path(exeDir) / ('test-%s' % label)
        budowa = kompiluj(P / 'Test.cpp', exe, flags)
        wynik['profiles'].setdefault(label, {})['compile'] = budowa.returncode
        zapisz()
        if budowa.returncode != 0:
            raise SystemExit('kompilacja [%s] nie wyszla:\n%s' % (label, budowa.stderr[-4000:]))
        t = krok('trasa216 [%s]' % label, [str(exe), str(urls)], env=env)
        print(t.stdout.strip() + '  [' + label + ']')
        wynik['profiles'][label]['run'] = t.returncode

    # --- 5. kontrole negatywne -------------------------------------------
    # N1a: stary panel (v3.56.2) w JS - adres "dlugiej" nie pasuje do strumienia
    stary = out / 'panel-stary'
    stary.mkdir()
    t = krok('N1a panel-test na starym panelu', ['node', str(P / 'panel_test.mjs'),
                                                str(P / 'panel' / 'panel-stary.html'),
                                                str(stary)], oczekiwany=1)
    if 'bledow=2' not in t.stdout:
        raise SystemExit('N1a: oczekiwano dwoch bledow w starym panelu, jest:\n%s' % t.stdout)
    wynik['kontrole_negatywne']['N1a_stary_panel_js'] = t.stdout.strip().split('\n')[-1]
    print('PASS N1a stary panel wywalil panel_test: ' + wynik['kontrole_negatywne']['N1a_stary_panel_js'])

    # N1b: te same stare adresy w PRAWDZIWEJ trasie (401 na dlugiej)
    exe = Path(exeDir) / 'test-native'
    t = krok('N1b stary panel -> trasa', [str(exe), str(stary / 'urls.txt')], oczekiwany=1)
    if 'bledow=2' not in t.stdout:
        raise SystemExit('N1b: oczekiwano dwoch bledow trasy, jest:\n%s' % t.stdout)
    wynik['kontrole_negatywne']['N1b_stary_panel_trasa'] = t.stdout.strip().split('\n')[-1]
    print('PASS N1b stary panel wywalil trase: ' + wynik['kontrole_negatywne']['N1b_stary_panel_trasa'])

    # N2: stary naglowek (bez Content-Disposition) -> brak nazwy pliku
    tmp = Path(tempfile.mkdtemp(prefix='n2-216-'))
    try:
        kopia = tmp / 'Test.cpp'
        shutil.copyfile(P / 'Test.cpp', kopia)
        exe = tmp / 'test-stary-naglowek'
        include = ['-I' + str(D213), '-I' + str(P), '-I' + str(D213 / 'host'),
                   '-I' + str(D211), '-I' + str(D211 / 'host'),
                   '-I' + str(D212), '-I' + str(D212 / 'host'), '-I' + str(FW)]
        budowa = kompiluj(kopia, exe, PROFILE[0][1], include)
        if budowa.returncode != 0:
            raise SystemExit('N2: kompilacja ze starym naglowkiem nie wyszla:\n%s'
                             % budowa.stderr[-3000:])
        t = krok('N2 stary naglowek', [str(exe), str(urls)], oczekiwany=1)
        if 'bledow=4' not in t.stdout:
            raise SystemExit('N2: oczekiwano czterech bledow (brak nazwy pliku), jest:\n%s'
                             % t.stdout)
        wynik['kontrole_negatywne']['N2_stary_naglowek'] = t.stdout.strip().split('\n')[-1]
        print('PASS N2 stary naglowek wywalil trase: '
              + wynik['kontrole_negatywne']['N2_stary_naglowek'])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # N3/N4: celowo zepsuty panel w JS
    for kod, stara, nowa, opis in (
            ('N3', 'id="histBtn"', 'id="histBtm"', 'literowka w id przycisku'),
            ('N4', 'bilet.length !== 32', 'bilet.length !== 31', 'zla dlugosc biletu')):
        tmp = Path(tempfile.mkdtemp(prefix='n-216-'))
        try:
            tekst = (P / 'panel' / 'panel.html').read_text(encoding='utf-8')
            if tekst.count(stara) != 1:
                raise SystemExit('%s: nie ma dokladnie jednego "%s"' % (kod, stara))
            cel = tmp / 'panel.html'
            cel.write_text(tekst.replace(stara, nowa, 1), encoding='utf-8')
            t = krok('%s %s' % (kod, opis),
                     ['node', str(P / 'panel_test.mjs'), str(cel), str(tmp)], oczekiwany=1)
            wynik['kontrole_negatywne']['%s_%s' % (kod, opis.replace(' ', '_'))] = \
                t.stdout.strip().split('\n')[-1]
            print('PASS %s %s -> panel_test czerwony (%s)'
                  % (kod, opis, wynik['kontrole_negatywne']['%s_%s' % (kod, opis.replace(' ', '_'))]))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    wynik['status'] = 'PASS'
    zapisz()
    print('PASS brama216 sprawdzen_w_krokach=%d profili=%d kontrol=%d'
          % (len(wynik['kroki']), len(wynik['profiles']), len(wynik['kontrole_negatywne'])))


if __name__ == '__main__':
    try:
        main()
    except Exception as blad:  # noqa: BLE001
        wynik['status'] = 'FAIL'
        wynik['error'] = str(blad)
        zapisz()
        raise

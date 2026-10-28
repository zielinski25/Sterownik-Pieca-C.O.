#!/usr/bin/env python3
"""219: ostateczna brama pliku do wgrania.

Co robi, po kolei:
  1. buduje od zera trzy stany pliku (v3.55.8, v3.56.3, v3.56.4) i sprawdza SHA,
  2. puszcza ten sam audyt na kazdym z nich - KAZDY stan musi dostac inna,
     oczekiwana odpowiedz:
       v3.55.8 - brak wszystkich poprawek (matryca B16),
       v3.56.3 - martwe trasy /history/long i /history/link (B13, B14),
       v3.56.4 - wszystko zielone (16 kontroli),
  3. psuje v3.56.4 na 10 sposobow - kazde zepsucie MUSI zrobic audyt czerwonym.

Wynik: WYNIKI/runNNN/wynik.json.
"""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile

P = Path(__file__).resolve().parent
R = P.parents[1]
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'
BAZA = FW / 'main_centrala.cpp'

sys.path.insert(0, str(P))
import mutanty as M  # noqa: E402

subprocess.run([sys.executable, '-B', str(P / 'build.py')], check=True)

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

wynik = {
    'status': 'RUNNING',
    'stage': 219,
    'release_ready': False,
    'main_changed': False,      # ten etap nie zmienia firmware
    'device_touched': False,
    'firmware': 'v3.56.4 (plik z etapu 218 - bez zmian w tym etapie)',
    'stany': {},
    'mutacje': {},
}


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2, ensure_ascii=False) + '\n')


def audyt(plik_cpp, katalog_json, js_katalog=None, baza=True):
    cmd = [sys.executable, '-B', str(P / 'audyt.py'), str(plik_cpp),
           '--wynik', str(katalog_json)]
    if baza:
        cmd += ['--baza', str(BAZA)]
    if js_katalog:
        cmd += ['--js-katalog', str(js_katalog)]
    t = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    raport = None
    if Path(katalog_json).exists():
        raport = json.loads(Path(katalog_json).read_text(encoding='utf-8'))
    return t, raport


def kody_czerwone(raport):
    return sorted(c['kod'] for c in raport['checks'] if not c['ok']) if raport else []


OCZEKIWANE = {
    # stan: (czy ma byc zielony, kody ktore MUSZA byc czerwone, kody ktore NIE moga)
    'v3.56.4': (True, [], []),
    # v3.56.3: wersja jeszcze stara (B11), trasy jeszcze gole (B13, B14), matryca (B16).
    'v3.56.3': (False, ['B11', 'B13', 'B14', 'B16'],
                ['B01', 'B02', 'B03', 'B04', 'B06', 'B07', 'B08', 'B09', 'B10', 'B12', 'B15']),
    'v3.55.8': (False, ['B16'], []),
}

try:
    with tempfile.TemporaryDirectory(prefix='brama219-') as td:
        td = Path(td)
        stany = td / 'stany'
        subprocess.run([sys.executable, '-B', str(P / 'build.py'), str(stany)], check=True)
        wynik['stany'] = json.loads((stany / 'stany.json').read_text(encoding='utf-8'))
        zapisz()

        # ---- 2. ten sam audyt na trzech stanach --------------------------
        for stan, (ma_byc_zielony, musza, nie_moga) in OCZEKIWANE.items():
            plik = stany / ('main_centrala-%s.cpp' % stan)
            t, raport = audyt(plik, out / ('audyt-%s.json' % stan), td / ('js-%s' % stan))
            czerwone = kody_czerwone(raport)
            wpis = {'returncode': t.returncode, 'zielony': raport['ok'] if raport else None,
                    'kontrol': raport['kontrol'] if raport else 0, 'czerwone': czerwone}
            wynik['stany'][stan] = wpis
            zapisz()
            assert raport is not None, (stan, t.stdout[-2000:], t.stderr[-2000:])
            assert t.returncode == (0 if ma_byc_zielony else 1), (stan, t.returncode)
            assert raport['ok'] is ma_byc_zielony, (stan, czerwone)
            for kod in musza:
                assert kod in czerwone, (stan, 'brak oczekiwanej czerwonej kontroli', kod, czerwone)
            for kod in nie_moga:
                assert kod not in czerwone, (stan, 'nieoczekiwana czerwona kontrola', kod, czerwone)
            if ma_byc_zielony:
                print('PASS audyt219 %s: %d kontroli, wszystkie zielone' % (stan, raport['kontrol']))
            else:
                print('PASS audyt219 %s: czerwony na %s (oczekiwane)' % (stan, ', '.join(czerwone)))

        # ---- 3. mutacje v3.56.4 ------------------------------------------
        tekst = (stany / 'main_centrala-v3.56.4.cpp').read_text(encoding='utf-8')
        for nazwa, opis, podmiany, oczekiwane in M.MUTANTY:
            zmutowany, blad = M.zastosuj(tekst, podmiany)
            if nazwa == 'M10_konce_linii_crlf' and blad is None:
                zmutowany = zmutowany.replace('\n', '\r\n')
            if zmutowany is None:
                raise SystemExit('mutant %s: %s' % (nazwa, blad))
            plik = td / ('mutant-%s.cpp' % nazwa)
            plik.write_text(zmutowany, encoding='utf-8', newline='')
            t, raport = audyt(plik, td / ('mutant-%s.json' % nazwa), None)
            czerwone = kody_czerwone(raport)
            wpis = {'opis': opis, 'returncode': t.returncode, 'czerwone': czerwone,
                    'oczekiwane': oczekiwane}
            wynik['mutacje'][nazwa] = wpis
            zapisz()
            assert raport is not None, (nazwa, t.stderr[-2000:])
            assert t.returncode != 0, (nazwa, 'audyt nie zauwazyl mutacji')
            brak = [k for k in oczekiwane if k not in czerwone]
            assert not brak, (nazwa, 'nie czerwone:', brak, czerwone)
            print('PASS mutant %s czerwony na %s (%s)' % (nazwa, ', '.join(czerwone), opis))

    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
print('PASS brama219 stanow=%d mutantow=%d kontroli_na_stan=%d'
      % (len(wynik['stany']), len(wynik['mutacje']), wynik['stany']['v3.56.4']['kontrol']))

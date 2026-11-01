#!/usr/bin/env python3
"""220: brama kontraktu z prawdziwa biblioteka.

  1. kontrakt.py na PRAWDZIWYCH naglowkach (SHA sprawdzany) - 7 kontroli,
  2. 10 mutacji: kazda zmienia jedna rzecz w naszym kodzie albo w kopii
     naglowka biblioteki i MUSI zapalic wlasciwa kontrole.

Wynik: WYNIKI/runNNN/wynik.json.
"""
from pathlib import Path
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

sys.path.insert(0, str(P))
import mutanty as M  # noqa: E402

BIBLIOTEKA = {
    'ESPAsyncWebServer.h': N / 'cykl-http139' / 'zrodla' / 'ESPAsyncWebServer.h',
    'WebResponseImpl.h': N / 'cykl-http139' / 'zrodla' / 'WebResponseImpl.h',
    'WebHandlerImpl.h': N / 'naglowki-upload206' / 'upstream' / 'WebHandlerImpl.h',
    'literals.h': N / 'naglowki-upload206' / 'upstream' / 'literals.h',
}
NASZE = {
    'HistoriaPobranie.h': N / 'od-przycisku216' / 'HistoriaPobranie.h',
    'HistoriaBilety.h': N / 'panel-pobrania213' / 'HistoriaBilety.h',
    'HistoriaPin.h': N / 'wysylka-zapinka211' / 'HistoriaPin.h',
}

base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / ('run%03d' % i)).exists():
    i += 1
out = base / ('run%03d' % i)
out.mkdir()

wynik = {
    'status': 'RUNNING',
    'stage': 220,
    'release_ready': False,
    'main_changed': False,
    'device_touched': False,
    'firmware': 'v3.56.4 (bez zmian w tym etapie)',
    'kontrol': {},
    'mutacje': {},
}


def zapisz():
    (out / 'wynik.json').write_text(json.dumps(wynik, indent=2, ensure_ascii=False) + '\n')


def uruchom(flagi=(), wynik_json=None):
    cmd = [sys.executable, '-B', str(P / 'kontrakt.py')]
    if wynik_json:
        cmd += ['--wynik', str(wynik_json)]
    cmd += list(flagi)
    t = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    raport = json.loads(Path(wynik_json).read_text(encoding='utf-8')) if wynik_json and \
        Path(wynik_json).exists() else None
    return t, raport


def czerwone(raport):
    return sorted(c['kod'] for c in raport['checks'] if not c['ok']) if raport else []


try:
    # ---- 1. prawdziwe naglowki --------------------------------------
    t, raport = uruchom(wynik_json=out / 'kontrakt.json')
    assert raport is not None, (t.stdout[-2000:], t.stderr[-2000:])
    wynik['kontrol'] = {'returncode': t.returncode, 'ok': raport['ok'],
                        'kontrol': raport['kontrol'], 'czerwone': czerwone(raport),
                        'wywolan': len(raport['wywolania']),
                        'model_inny': sum(1 for w in raport['wywolania']
                                          if w.get('model_zgodny') is False)}
    zapisz()
    assert t.returncode == 0, (t.stdout[-3000:],)
    print('PASS kontrakt220 %d kontroli, %d wywolan biblioteki, model rozny w %d'
          % (raport['kontrol'], len(raport['wywolania']), wynik['kontrol']['model_inny']))

    # ---- 2. mutacje --------------------------------------------------
    with tempfile.TemporaryDirectory(prefix='k220-') as td:
        td = Path(td)
        for nazwa, opis, plik, podmiany, oczekiwane, flagi in M.MUTANTY:
            katalog_bib = td / ('%s-bib' % nazwa)
            katalog_nasz = td / ('%s-nasz' % nazwa)
            for d in (katalog_bib, katalog_nasz):
                d.mkdir()
                for n, zrodlo in BIBLIOTEKA.items():
                    shutil.copyfile(zrodlo, d / n)
                for n, zrodlo in NASZE.items():
                    shutil.copyfile(zrodlo, d / n)
            cel = katalog_bib / plik if plik in BIBLIOTEKA else katalog_nasz / plik
            tekst = cel.read_text(encoding='utf-8')
            nowy, blad = M.zastosuj(tekst, podmiany)
            if nowy is None:
                raise SystemExit('mutant %s: %s' % (nazwa, blad))
            cel.write_text(nowy, encoding='utf-8')
            t, raport = uruchom(['--zrodla', str(katalog_bib), '--nasze', str(katalog_nasz),
                                 *flagi], td / ('%s.json' % nazwa))
            czerw = czerwone(raport)
            wpis = {'opis': opis, 'plik': plik, 'oczekiwane': oczekiwane,
                    'czerwone': czerw, 'returncode': t.returncode}
            wynik['mutacje'][nazwa] = wpis
            zapisz()
            assert raport is not None, (nazwa, t.stderr[-2000:])
            assert t.returncode != 0, (nazwa, 'kontrakt nie zauwazyl mutacji')
            brak = [k for k in oczekiwane if k not in czerw]
            assert not brak, (nazwa, 'nie czerwone:', brak, czerw)
            print('PASS mutant %s czerwony na %s (%s)' % (nazwa, ', '.join(czerw), opis))

    wynik['status'] = 'PASS'
except Exception as blad:  # noqa: BLE001
    wynik['status'] = 'FAIL'
    wynik['error'] = str(blad)
    zapisz()
    raise

zapisz()
print('PASS brama220 kontrol=%d mutantow=%d wywolan=%d'
      % (wynik['kontrol']['kontrol'], len(wynik['mutacje']), wynik['kontrol']['wywolan']))

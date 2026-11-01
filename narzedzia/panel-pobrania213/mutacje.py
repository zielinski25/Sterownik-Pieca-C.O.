#!/usr/bin/env python3
"""213: mutacje panelu i biletow.

Kazdy mutant psuje jedna, konkretna obietnice etapu 213 (bilet jednorazowy,
bilet przypiety do strumienia, termin waznosci, brak tokenu w panelu).
Mutant musi sie skompilowac i MUSI zostac wykryty przez testy.
Mutacje rownowazne nie sa tu wpisywane.
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

BILE = 'HistoriaBilety.h'
TRASA = 'HistoriaPobranie.h'
EXT = 'Extracted213.inc'

pliki = [BILE, TRASA, EXT, 'Test.cpp', 'host/Serwer213.h', 'host/Model213.h']

flagi = ['-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
         '-fno-omit-frame-pointer', '-pthread', '-fno-exceptions', '-fno-rtti', '-Wall', '-Wextra']

mutanty = [
    ('bilet-nie-jednorazowy', [(BILE,
        '    bilety_[trafienie].zajety = false;\n', '')]),
    ('bilet-na-kazdy-strumien', [(BILE,
        'unsigned char inny = static_cast<unsigned char>(bilety_[i].zajety && bilety_[i].strumien == s ? 0 : 1);',
        'unsigned char inny = static_cast<unsigned char>(bilety_[i].zajety ? 0 : 1);')]),
    ('bilet-bez-terminu', [(BILE,
        '      if (static_cast<std::int32_t>(teraz - bilety_[i].wygasa) >= 0) inny = 1;  // wygasl\n', '')]),
    ('bilet-krotki-kod-wystarczy', [(BILE,
        'if (kod == nullptr || dlugosc != kZnakow || zegar_ == nullptr) return false;',
        'if (kod == nullptr || dlugosc < 8 || zegar_ == nullptr) return false;')]),
    ('bilet-wypycha-najnowszy', [(BILE,
        'if (static_cast<std::int32_t>(bilety_[i].wygasa - najwczesniej) < 0) {',
        'if (static_cast<std::int32_t>(bilety_[i].wygasa - najwczesniej) > 0) {')]),
    # Anchor obejmuje naglowek funkcji, bo ten sam warunek jest tez w obsluz().
    ('link-bez-sprawdzenia-metody', [(TRASA,
        '  void link(AsyncWebServerRequest *request) {\n'
        '    if (request == nullptr) return;\n'
        '    if (request->method() != HTTP_GET) {',
        '  void link(AsyncWebServerRequest *request) {\n'
        '    if (request == nullptr) return;\n'
        '    if (false) {')]),
    ('link-bez-tokenu', [(TRASA,
        '    if (token_ == nullptr || *token_ == \'\\0\') {\n'
        '      odpowiedzBledu(request, 503, "Pobieranie historii wylaczone (brak tokenu w secrets.h)", nullptr, nullptr);\n'
        '      return;\n'
        '    }\n'
        '    if (bilety_ == nullptr) {',
        '    if (false) {\n'
        '      odpowiedzBledu(request, 503, "Pobieranie historii wylaczone (brak tokenu w secrets.h)", nullptr, nullptr);\n'
        '      return;\n'
        '    }\n'
        '    if (bilety_ == nullptr) {')]),
    ('bilet-niesprawdzany-w-trasie', [(TRASA,
        '      if (!bilety_->zuzyj(bilet.c_str(), bilet.length(),',
        '      if (false && !bilety_->zuzyj(bilet.c_str(), bilet.length(),')]),
    ('zly-bearer-ratuje-bilet', [(TRASA,
        '        odpowiedzBledu(request, 401, "Brak albo bledny token", "WWW-Authenticate", "Bearer");\n'
        '        return;\n'
        '      }\n'
        '    } else if (bilety_ != nullptr && request->hasParam("k")) {',
        '        przezBilet = true;\n'
        '      }\n'
        '    } else if (bilety_ != nullptr && request->hasParam("k")) {')]),
    ('link-bez-cache-control', [(TRASA,
        '    if (r != nullptr) r->addHeader("Cache-Control", "no-store");\n', '')]),
    ('panel-ze-stalym-tokenem', [(EXT,
        "  var info = document.getElementById('histInfo');",
        "  var info = document.getElementById('histInfo'); /* HISTORIA_TOKEN */")]),
    ('panel-bez-sprawdzenia-dlugosci', [(EXT,
        "if (bilet.length !== 32) throw new Error('centrala nie dała biletu');",
        "if (false) throw new Error('centrala nie dała biletu');")]),
]

oryginal = {n: (P / n).read_text(encoding='utf-8') for n in pliki}

INCLUDE = ['-I' + str(S211), '-I' + str(S211 / 'host'), '-I' + str(S212),
           '-I' + str(S212 / 'host'), '-I' + str(FW)]


def zbuduj(katalog):
    cmd = ['c++', *flagi, '-I' + str(katalog), '-I' + str(katalog / 'host'), *INCLUDE,
           str(katalog / 'Test.cpp'), '-o', str(katalog / 'test')]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=600)


przypadki = []
with tempfile.TemporaryDirectory() as td:
    d = Path(td)
    for n, s in oryginal.items():
        (d / n).parent.mkdir(parents=True, exist_ok=True)
        (d / n).write_text(s, encoding='utf-8')
    b = zbuduj(d)
    assert b.returncode == 0, b.stderr[-3000:]
    t = subprocess.run([str(d / 'test')], capture_output=True, text=True, timeout=600, cwd=str(d),
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
    assert t.returncode == 0, ('KONTROLA', t.stdout[-2000:], t.stderr[-2000:])
    print('PASS mutacje213 kontrola: niemutowany kod przechodzi')

    for nazwa, pary in mutanty:
        for n, s in oryginal.items():
            (d / n).write_text(s, encoding='utf-8')
        for plik, a, b2 in pary:
            s = (d / plik).read_text(encoding='utf-8')
            assert s.count(a) >= 1, ('nie ma wzorca', nazwa, plik)
            (d / plik).write_text(s.replace(a, b2, 1), encoding='utf-8')
        kompilacja = zbuduj(d)
        assert kompilacja.returncode == 0, ('NIE KOMPILUJE', nazwa, kompilacja.stderr[-2000:])
        try:
            u = subprocess.run([str(d / 'test')], capture_output=True, text=True, timeout=180,
                               cwd=str(d),
                               env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
            rc, przyczyna = u.returncode, ('asan' if 'ERROR: AddressSanitizer' in u.stderr
                                           else 'wynik-testu')
            assert rc != 0, ('PRZEZYL', nazwa, u.stdout[-1500:])
        except subprocess.TimeoutExpired:
            rc, przyczyna = -9, 'zawieszenie'
        przypadki.append({'name': nazwa, 'compiled': True, 'detected': True,
                          'returncode': rc, 'reason': przyczyna})
        print('PASS mutacja213 wykryta: %s (%s)' % (nazwa, przyczyna))

wejscia = [P / n for n in pliki] + [P / 'mutacje.py']
(P / 'MUTACJE.json').write_text(json.dumps({
    'status': 'PASS',
    'profile': 'ASan+UBSan, LSan, -fno-exceptions',
    'kontrola': 'niemutowany kod przechodzi',
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in wejscia},
    'cases': przypadki,
}, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print('PASS mutacje213 skompilowanych i wykrytych', len(przypadki))

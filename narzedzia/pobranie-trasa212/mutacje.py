#!/usr/bin/env python3
"""212: mutacje trasy pobrania.

Kazdy mutant to mala, sensowna z punktu widzenia programisty zmiana, ktora
PSUJE obietnice trasy (np. zapowiedz dlugosci, ktorej plik nie ma; puszczenie
krotszego odczytu jako konca; brak zamkniecia zapinki). Mutant musi sie
skompilowac i MUSI zostac wykryty przez test. Mutacje rownowazne (takie,
ktore nic nie zmieniaja) nie sa tu wpisywane.
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

TRASA = 'HistoriaPobranie.h'
EXT = 'Extracted212.inc'

pliki = [TRASA, EXT, 'Test.cpp', 'host/Serwer212.h', 'HistoriaPin.h']
Z211 = {'HistoriaPin.h': S211 / 'HistoriaPin.h'}

flagi = ['-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
         '-fno-omit-frame-pointer', '-pthread', '-fno-exceptions', '-fno-rtti', '-Wall', '-Wextra']

mutanty = [
    ('metoda-zawsze-dozwolona', [(TRASA,
        'if (request->method() != HTTP_GET) {', 'if (false) {')]),
    ('token-nie-sprawdzany', [(TRASA,
        '        !tokenZgodny(podany, podanaDlugosc, token_)) {',
        '        false) {')]),
    # Mutant: brak sprawdzenia dlugosci + porownywanie tylko prefiksu. Samo
    # skrocenie zasiegu nic nie zmienia (dlugosc jest sprawdzana osobno), wiec
    # dopiero ta PARA zmian daje realna luke. Test 04 podaje krotszy prefiks.
    ('token-prefiks-wystarczy', [
        (TRASA, '  unsigned char roznica = static_cast<unsigned char>(n != m ? 1 : 0);',
                '  unsigned char roznica = 0;'),
        (TRASA, '  const std::size_t zasieg = (n > m) ? n : m;',
                '  const std::size_t zasieg = (n < m) ? n : m;')]),
    ('zajetosc-nie-ustawiana', [(TRASA,
        '    zajeta_ = true;\n    request->send(odp);',
        '    request->send(odp);')]),
    ('dlugosc-o-jeden-za-duzo', [(TRASA,
        '    _contentLength = static_cast<size_t>(pin ? pin->length() : 0);',
        '    _contentLength = static_cast<size_t>(pin ? pin->length() : 0) + 1;')]),
    ('krotki-odczyt-jako-koniec', [(TRASA,
        '    if (r.code != historia211::ReadCode::End) awaria_ = true;  // Short/Fault',
        '    if (r.code == historia211::ReadCode::Fault) awaria_ = true;')]),
    ('zrodlo-zawsze-wazne', [(TRASA,
        '  bool _sourceValid() const override { return !awaria_ && pin_ != nullptr && pin_->active(); }',
        '  bool _sourceValid() const override { return true; }')]),
    # Mutant: odpowiedz nie zwalnia znacznika zajetosci, wiec po pierwszym
    # pobraniu trasa zostaje "zajeta" na zawsze (drugie i kolejne = 503).
    ('zajetosc-nie-zwalniana', [(TRASA,
        '    zajeta_ = false;\n  }', '  }')]),
    ('zamknij-bez-mutexa', [(EXT,
        '  bool zamknij() override {\n'
        '    if (!logMutex) return false;\n'
        '    return xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) == pdTRUE;\n'
        '  }\n'
        '  void otworz() override { xSemaphoreGive(logMutex); }',
        '  bool zamknij() override { return true; }\n'
        '  void otworz() override {}')]),
    ('magazyn-zawsze-gotowy', [(EXT,
        'bool magazynGotowy() override { return historiaDostepGotowy() && littlefsReady; }',
        'bool magazynGotowy() override { return true; }')]),
    ('za-duzy-plik-jako-200', [(TRASA,
        'odpowiedzBledu(request, 413, "Historia wieksza niz limit", nullptr, nullptr);',
        'odpowiedzBledu(request, 200, "Historia wieksza niz limit", nullptr, nullptr);')]),
    ('zla-metoda-jako-200', [(TRASA,
        'odpowiedzBledu(request, 405, "Metoda niedozwolona", "Allow", "GET");',
        'odpowiedzBledu(request, 200, "Metoda niedozwolona", "Allow", "GET");')]),
]

oryginal = {n: (Z211[n] if n in Z211 else P / n).read_text(encoding='utf-8') for n in pliki}


def zbuduj(katalog):
    cmd = ['c++', *flagi, '-I' + str(katalog), '-I' + str(katalog / 'host'), '-I' + str(FW),
           '-I' + str(S211), '-I' + str(S211 / 'host'), str(katalog / 'Test.cpp'),
           '-o', str(katalog / 'test')]
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
    print('PASS mutacje212 kontrola: niemutowany kod przechodzi')

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
        print('PASS mutacja212 wykryta: %s (%s)' % (nazwa, przyczyna))

wejscia = [(Z211[n] if n in Z211 else P / n) for n in pliki] + [P / 'mutacje.py']
(P / 'MUTACJE.json').write_text(json.dumps({
    'status': 'PASS',
    'profile': 'ASan+UBSan, LSan, -fno-exceptions',
    'kontrola': 'niemutowany kod przechodzi',
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in wejscia},
    'cases': przypadki,
}, indent=2) + '\n', encoding='utf-8')
print('PASS mutacje212 skompilowanych i wykrytych', len(przypadki))

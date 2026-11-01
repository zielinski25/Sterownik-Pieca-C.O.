from pathlib import Path
import hashlib
import json
import os
import resource
import shutil
import subprocess
import tempfile

P = Path(__file__).resolve().parent
R = P.parents[1]
FW = R / 'firmware-patch' / 'v3.55.8-blokada-fs-po-starcie'

PIN = 'HistoriaPin.h'
EXT = 'Extracted211.inc'

# Kazda mutacja to lista par (plik, tekst, zamiana). Mutant MUSI sie skompilowac
# i MUSI zostac wykryty przez ktorys test. Mutacje rownowazne nie sa tu wpisywane.
mutanty = [
    ('dlugosc-ignoruje-znacznik-zapisu', [(PIN,
        'length_ = (physical < committed) ? physical : committed;',
        'length_ = physical;')]),
    ('dlugosc-ignoruje-rozmiar-pliku', [(PIN,
        'length_ = (physical < committed) ? physical : committed;',
        'length_ = committed;')]),
    ('krotki-odczyt-jako-koniec', [(PIN,
        'if (n == 0) {  // plik skonczyl sie przed zapowiedzia - nie koniec\n'
        '        broken_ = true;\n'
        '        return ReadResult(ReadCode::Short, got);\n'
        '      }',
        'if (n == 0) { return ReadResult(ReadCode::End, got); }')]),
    ('blad-odczytu-jako-koniec', [(PIN,
        'if (n < 0 || io_->error() != 0) {\n'
        '        broken_ = true;\n'
        '        return ReadResult(ReadCode::Fault, got);\n'
        '      }',
        'if (n < 0 || io_->error() != 0) { return ReadResult(ReadCode::End, got); }')]),
    ('brak-limitu-rozmiaru', [(PIN, 'if (length_ > maxBytes) {', 'if (false) {')]),
    ('zero-potwierdzone-przyjete', [(PIN, 'if (length_ == 0) {', 'if (false) {')]),
    ('brak-fstat', [(PIN,
        'if (io.stat(fd, &st) != 0 || io.error() != 0 || !S_ISREG(st.st_mode) ||\n'
        '        st.st_size <= 0) {',
        'if (false) {')]),
    ('zamkniecie-nie-zamyka', [(PIN,
        'const int rc = io->close(fd);', 'const int rc = 0;')]),
    ('krotki-zapis-przyjety', [(EXT,
        'if (zapisano != odczyt.bytes) {', 'if (false) {')]),
    ('nieudane-cialo-bez-przerwania', [(EXT,
        '    if (!cialoOk) goto tgDocHistSSendDone;', '    (void)cialoOk;')]),
    ('brak-zapinki-przyjety', [(EXT,
        'if (zapinka != historia211::OpenCode::Ok) {', 'if (false) {')]),
    ('otwarcie-bez-blokady-pisarza', [
        (EXT, 'if (!logMutex || xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;',
         'if (!logMutex) return false;'),
        (EXT, 'xSemaphoreGive(logMutex);', ';')]),
]

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
pliki = ['Test.cpp', PIN, EXT, 'host/Arduino.h', 'host/RamCalls.h', 'host/Firmware211.h']
oryginal = {n: (P / n).read_text(encoding='utf-8') for n in pliki}
flagi = ['-std=c++17', '-O1', '-g', '-pthread', '-fno-exceptions', '-fno-rtti',
         '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie']


def zbuduj(katalog):
    cmd = ['c++', *flagi, '-I' + str(katalog), '-I' + str(katalog / 'host'), '-I' + str(FW),
           str(katalog / 'Test.cpp'), '-o', str(katalog / 'test')]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=600)


przypadki = []
with tempfile.TemporaryDirectory() as td:
    d = Path(td)
    # 0) kontrola: niemutowany kod musi przejsc w tym samym ukladzie
    for n, s in oryginal.items():
        (d / n).parent.mkdir(parents=True, exist_ok=True)
        (d / n).write_text(s, encoding='utf-8')
    b = zbuduj(d)
    assert b.returncode == 0, b.stderr[-3000:]
    t = subprocess.run([str(d / 'test')], capture_output=True, text=True, timeout=600, cwd=str(d),
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
    assert t.returncode == 0, ('KONTROLA', t.stdout[-2000:], t.stderr[-2000:])
    print('PASS mutacje211 kontrola: niemutowany kod przechodzi')

    # 1) mutanci
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
            u = subprocess.run([str(d / 'test')], capture_output=True, text=True, timeout=180, cwd=str(d),
                               env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
            rc, przyczyna = u.returncode, ('asan' if 'ERROR: AddressSanitizer' in u.stderr else 'wynik-testu')
            assert rc != 0, ('PRZEZYL', nazwa, u.stdout[-1500:])
        except subprocess.TimeoutExpired:
            rc, przyczyna = -9, 'zawieszenie'
        przypadki.append({'name': nazwa, 'compiled': True, 'detected': True,
                          'returncode': rc, 'reason': przyczyna})
        print('PASS mutacja211 wykryta: %s (%s)' % (nazwa, przyczyna))

wejscia = [P / n for n in pliki] + [P / 'mutacje.py']
(P / 'MUTACJE.json').write_text(json.dumps({
    'status': 'PASS',
    'profile': 'ASan+UBSan, LSan, -fno-exceptions',
    'kontrola': 'niemutowany kod przechodzi',
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in wejscia},
    'cases': przypadki,
}, indent=2) + '\n', encoding='utf-8')
print('PASS mutacje211 skompilowanych i wykrytych', len(przypadki))

from pathlib import Path
import tempfile, subprocess, hashlib, json, os
P = Path(__file__).resolve().parent
R = P.parents[1]
N = P.parent
A1199 = N / 'odpowiedz-http199'
D203 = N / 'odpowiedz-odroczona203'
S207 = N / 'pobieranie-historia207'
T208 = N / 'pobieranie-http208'
H197 = N / 'uchwyty-runtime197'
M198 = N / 'most-runtime198'
U196 = N / 'blokada-startu-fs196/upstream'
W206 = N / 'naglowki-upload206'
X205 = N / 'parser-upload205'

subprocess.run(['python3', '-B', str(P / 'build.py')], check=True)
base = P / 'WYNIKI'
base.mkdir(exist_ok=True)
i = 1
while (base / f'run{i:03d}').exists():
    i += 1
out = base / f'run{i:03d}'
out.mkdir()

inputs = [*P.glob('*.h'), *P.glob('*.inc'), *P.glob('*.cpp'), *P.glob('*.py'), P / 'host/Arduino.h',
          T208 / 'DownloadAdapter.h', T208 / 'SnapshotResponse.h', T208 / 'Transport208.inc', T208 / 'ResponseTail208.inc',
          T208 / 'Abstract.inc', T208 / 'CheckedFileBody.inc', S207 / 'Snapshots.h', H197 / 'RuntimeFilesystem.h',
          M198 / 'Model197.inc', U196 / 'FS.cpp', A1199 / 'CheckedFileBody.h', D203 / 'Methods.inc', D203 / 'Fields.inc',
          D203 / 'Factory.inc', D203 / 'API.inc', W206 / 'Methods206.inc', W206 / 'Parser206.inc', W206 / 'Enums206.inc',
          W206 / 'AsyncWebHeader.inc', W206 / 'upstream/literals.h', W206 / 'upstream/WebHandlers.cpp',
          W206 / 'upstream/WebServer.cpp', W206 / 'candidate/WebServer.cpp', H197 / 'upstream/ESPAsyncWebServer.h',
          X205 / 'API205.inc', X205 / 'Fields205.inc']
r = {'status': 'RUNNING', 'release_ready': False, 'main_changed': False,
     'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}, 'profiles': {}}


def save():
    (out / 'wynik.json').write_text(json.dumps(r, indent=2) + '\n')


try:
    with tempfile.TemporaryDirectory() as td:
        for label, flags in [('native', []),
                             ('asan-ubsan', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                             '-fno-omit-frame-pointer', '-no-pie'])]:
            for flow in [0, 1]:
                exe = Path(td) / 'test'
                cmd = ['c++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pthread',
                       '-fno-exceptions', '-fno-rtti', *flags, '-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT=' + str(flow),
                       *['-I' + str(x) for x in [P / 'host', P, T208, S207, H197, H197 / 'upstream', M198, U196,
                                                 A1199, D203, W206, W206 / 'upstream', X205]],
                       P / 'Test.cpp', U196 / 'FS.cpp', '-o', exe]
                c = subprocess.run(list(map(str, cmd)), capture_output=True, text=True, timeout=300)
                assert c.returncode == 0, c.stderr
                t = subprocess.run([exe], capture_output=True, text=True, timeout=180,
                                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
                r['profiles'][label + '-flow' + str(flow)] = {'stdout': t.stdout, 'stderr': t.stderr, 'returncode': t.returncode}
                save()
                assert t.returncode == 0, (t.stdout, t.stderr)
                print(t.stdout.strip())
    r['status'] = 'PASS'
    save()
    print(out)
except Exception as e:
    r['status'] = 'FAIL'
    r['error'] = str(e)
    save()
    raise

from pathlib import Path
import tempfile, subprocess, json, hashlib, os, resource
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

# Each mutation must compile and be caught by at least one group.
mutants = [
 ('no-credentials-check', 'DownloadRoute.h',
  'if (r.authType() != AUTH_BEARER || !equalConstantTime(', 'if (false && !equalConstantTime('),
 ('scheme-not-checked', 'DownloadRoute.h',
  'if (r.authType() != AUTH_BEARER || !equalConstantTime(', 'if (!equalConstantTime('),
 ('token-ignored', 'DownloadRoute.h',
  'return diff == 0;', 'return true || diff == 0;'),
 ('compare-first-byte-only', 'DownloadRoute.h',
  'const std::size_t n = (an < bn) ? bn : an;', 'const std::size_t n = 1;'),
 ('refusal-not-sent', 'DownloadRoute.h', '  r.send(code);\n', ''),
 ('method-not-checked', 'DownloadRoute.h',
  'if (r.method() != AsyncWebRequestMethod::HTTP_GET) {', 'if (false) {'),
 ('full-answered-as-download', 'DownloadRoute.h',
  '   case Adapter::Code::Full:\n    ++stats_.busy;\n    return fail(Outcome::Busy, 503, r);',
  '   case Adapter::Code::Full:\n    ++stats_.busy;\n    break;'),
 ('unavailable-answered-as-200', 'DownloadRoute.h',
  '   case Adapter::Code::Unavailable:\n    ++stats_.unavailable;\n    return fail(Outcome::Unavailable, 503, r);',
  '   case Adapter::Code::Unavailable:\n    ++stats_.unavailable;\n    break;'),
 ('admission-without-rights', 'DownloadRoute.h',
  'const auto begun = adapter_.begin(r, timeout_, true);', 'const auto begun = adapter_.begin(r, timeout_, false);'),
 ('route-prefix-not-exact', 'Test.cpp', 'AsyncURIMatcher::exact("/history")', 'AsyncURIMatcher::prefix("/history")'),
 ('handler-get-only', 'Test.cpp',
  '| AsyncWebRequestMethod::HTTP_POST |\n                     AsyncWebRequestMethod::HTTP_PUT | AsyncWebRequestMethod::HTTP_DELETE | AsyncWebRequestMethod::HTTP_HEAD',
  '| AsyncWebRequestMethod::HTTP_GET |\n                     AsyncWebRequestMethod::HTTP_GET | AsyncWebRequestMethod::HTTP_GET | AsyncWebRequestMethod::HTTP_GET'),
 ('response-before-send', 'Fixture209.inc', ' _assembled_headers = "HEAD";', ' _assembled_headers = "GET ";'),
]
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
cases = []
files = ['Test.cpp', 'DownloadRoute.h', 'Fixture209.inc', 'Real209Decl.inc', 'Real209Impl.inc']
original = {n: (P / n).read_text() for n in files}
include = ['-I' + str(x) for x in [P / 'host', P, T208, S207, H197, H197 / 'upstream', M198, U196, A1199, D203,
                                   W206, W206 / 'upstream', X205]]
with tempfile.TemporaryDirectory() as td:
    d = Path(td)
    for name, file, a, b in mutants:
        for n, s in original.items():
            (d / n).write_text(s)
        s = original[file]
        assert s.count(a) >= 1, (name, file, s.count(a))
        (d / file).write_text(s.replace(a, b, 1))
        cmd = ['c++', '-std=c++17', '-O1', '-g', '-pthread', '-fno-exceptions', '-fno-rtti',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
               '-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT=1', *include, d / 'Test.cpp', U196 / 'FS.cpp', '-o', d / 'test']
        t = subprocess.run(list(map(str, cmd)), capture_output=True, text=True, timeout=300)
        assert t.returncode == 0, (name, t.stderr[-2000:])
        t = subprocess.run([d / 'test'], capture_output=True, text=True, timeout=180,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
        assert t.returncode != 0, ('SURVIVED', name)
        cases.append({'name': name, 'compiled': True, 'detected': True, 'returncode': t.returncode,
                      'asan_error': 'ERROR: AddressSanitizer' in t.stderr})
inputs = [P / n for n in files] + [P / 'mutacje.py', T208 / 'DownloadAdapter.h', S207 / 'Snapshots.h',
                                   W206 / 'Methods206.inc', W206 / 'Parser206.inc', U196 / 'FS.cpp']
(P / 'MUTACJE.json').write_text(json.dumps({
    'status': 'PASS', 'profile': 'ASan+UBSan, LSan enabled, chunk-inflight on',
    'inputs': {str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
    'cases': cases}, indent=2) + '\n')
print('PASS route209 compiled mutants', len(cases))

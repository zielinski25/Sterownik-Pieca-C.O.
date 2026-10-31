from pathlib import Path
import hashlib, json, subprocess
P = Path(__file__).resolve().parent
R = P.parents[1]
N = P.parent
H197 = N / 'uchwyty-runtime197'
U206 = N / 'naglowki-upload206'
X205 = N / 'parser-upload205'
sha = lambda b: hashlib.sha256(b).hexdigest()

# 1. Prerequisites must be regenerated, never reused stale.
subprocess.run(['python3', '-B', str(X205 / 'build.py')], check=True, capture_output=True)
subprocess.run(['python3', '-B', str(U206 / 'build.py')], check=True, capture_output=True)

header_src = H197 / 'upstream/ESPAsyncWebServer.h'
server_src = U206 / 'candidate/WebServer.cpp'
handlers_src = U206 / 'upstream/WebHandlers.cpp'
meta = json.loads((U206 / 'upstream/SOURCES.json').read_text())
assert sha(header_src.read_bytes()) == json.loads((H197 / 'upstream/HEADER_SOURCE.json').read_text())['sha256']
assert sha(handlers_src.read_bytes()) == meta['WebHandlers.cpp']['sha256']

header = header_src.read_text()
server = server_src.read_text()
handlers = handlers_src.read_text()


def block(text, start, end, name):
    a = text.index(start)
    b = text.index(end, a) + len(end)
    assert a >= 0 and b > a, name
    return text[a:b]


def guard(snippet, source, name):
    assert snippet in source, name
    return snippet


# 2. Exact pieces of the pinned library.
method_composite = block(header, 'class WebRequestMethodComposite {', '\n};\n', 'WebRequestMethodComposite')
# The class body itself ends with "\n};" - keep only up to that terminator.
uri_matcher = block(header, 'class AsyncURIMatcher {', '\n};\n', 'AsyncURIMatcher')
uri_ctor_plain = block(server, 'AsyncURIMatcher::AsyncURIMatcher(String uri, uint16_t modifiers)', '\n}\n', 'ctor plain')
uri_ctor_typed = block(server, 'AsyncURIMatcher::AsyncURIMatcher(String uri, Type type, uint16_t modifiers)', '\n}\n', 'ctor typed')
uri_matches = block(server, 'bool AsyncURIMatcher::matches(AsyncWebServerRequest *request) const {', '\n}\n', 'matches')
attach = block(server, 'void AsyncWebServer::_attachHandler(AsyncWebServerRequest *request) {', '\n}\n', 'attachHandler')
can_handle = block(handlers, 'bool AsyncCallbackWebHandler::canHandle(AsyncWebServerRequest *request) const {', '\n}\n', 'canHandle')
handle_request = block(handlers, 'void AsyncCallbackWebHandler::handleRequest(AsyncWebServerRequest *request) {', '\n}\n', 'handleRequest')
filter_inline = guard('    return _filter == NULL || _filter(request);', header, 'filter inline body')

# 3. The extracted text must stay byte-identical to its source file.
for snippet, source, name in [
    (method_composite, header, 'method composite'),
    (uri_matcher, header, 'uri matcher'),
    (uri_ctor_plain, server, 'uri ctor plain'),
    (uri_ctor_typed, server, 'uri ctor typed'),
    (uri_matches, server, 'uri matches'),
    (attach, server, 'attach handler'),
    (can_handle, handlers, 'canHandle'),
    (handle_request, handlers, 'handleRequest'),
]:
    assert snippet in source, name

head = '''// LGPL-3.0-or-later, ESP32Async/ESPAsyncWebServer pinned c47917dcaba2acefb04277cd679f58a60b4fab30.
// Wyciagniete bez zmian przez build.py; kazdy fragment jest bajtowo rowny plikowi zrodlowemu.
// Czlon 1: klasy WebRequestMethodComposite i AsyncURIMatcher z naglowka.
'''
(P / 'Real209Decl.inc').write_text(head + method_composite + uri_matcher)
impl = '''// LGPL-3.0-or-later, ESP32Async/ESPAsyncWebServer pinned c47917dcaba2acefb04277cd679f58a60b4fab30.
// Wyciagniete bez zmian przez build.py; kazdy fragment jest bajtowo rowny plikowi zrodlowemu.
// Czlon 2: konstruktory i matches z WebServer.cpp (kandydat 206), AsyncWebServer::_attachHandler
// z WebServer.cpp, AsyncCallbackWebHandler::canHandle i handleRequest z WebHandlers.cpp.
'''
impl += uri_ctor_plain + '\n' + uri_ctor_typed + '\n' + uri_matches + '\n' + attach + '\n' + can_handle + '\n' + handle_request
(P / 'Real209Impl.inc').write_text(impl)

# 4. The host String shim must only add to the frozen 206 shim, never change what it already had.
base_host = (U206 / 'host/Arduino.h').read_text().splitlines()
own_host = (P / 'host/Arduino.h').read_text().splitlines()
it = iter(own_host)
for line in base_host:
    for candidate in it:
        if candidate == line:
            break
    else:
        raise AssertionError('206 host line lost: ' + line)

(P / 'EXTRACTION.json').write_text(json.dumps({
    'release_ready': False,
    'ref': 'c47917dcaba2acefb04277cd679f58a60b4fab30',
    'header_sha256': sha(header_src.read_bytes()),
    'server_candidate_sha256': sha(server_src.read_bytes()),
    'webhandlers_sha256': sha(handlers_src.read_bytes()),
    'extracted': {
        'AsyncURIMatcher (+WebRequestMethodComposite)': sha((method_composite + uri_matcher).encode()),
        'AsyncURIMatcher::matches + constructors': sha((uri_ctor_plain + uri_ctor_typed + uri_matches).encode()),
        'AsyncWebServer::_attachHandler': sha(attach.encode()),
        'AsyncCallbackWebHandler::canHandle + handleRequest': sha((can_handle + handle_request).encode()),
        'AsyncWebHandler::filter expression': sha(filter_inline.encode()),
    },
    'byte_identical_to_source': True,
    'host_string_shim': 'own host/Arduino.h is a superset of the frozen 206 shim (checked line by line)',
    'modeled': ['String shim', 'TCP client', 'HTTP header serialisation (marker HEAD)', 'error response body', 'FreeRTOS scheduling'],
}, indent=2) + '\n')
print('PASS 209 real matcher, real handler selection, real attach order extracted from pinned sources')

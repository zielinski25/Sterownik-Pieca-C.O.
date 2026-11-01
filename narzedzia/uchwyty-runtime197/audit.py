from pathlib import Path
import json,hashlib,re,subprocess
P=Path(__file__).resolve().parent;R=P.parents[1];F=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp'
assert hashlib.sha256(F.read_bytes()).hexdigest()=='3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31'
for f,m in [('WebResponses.cpp','SOURCES.json'),('ESPAsyncWebServer.h','HEADER_SOURCE.json')]:assert hashlib.sha256((P/'upstream'/f).read_bytes()).hexdigest()==json.loads((P/'upstream'/m).read_text())['sha256']
s=(P/'upstream/WebResponses.cpp').read_text();body=re.search(r'size_t AsyncFileResponse::_fillBuffer\(uint8_t \*data, size_t len\) \{[^}]+\}',s).group();assert 'return _content.read(data, len);' in body
h=(P/'upstream/ESPAsyncWebServer.h').read_text();constant=re.search(r'#define RESPONSE_TRY_AGAIN\s+(0x[0-9A-Fa-f]+)',h).group(1);assert int(constant,16)==0xffffffff
(P/'WebFill.inc').write_text('// Exact pinned method body, model class, real fs::File. LGPL-3.0-or-later.\n'+body+'\nstatic const uint32_t tryAgain197='+constant+';\n')
lines=F.read_text().splitlines();needles=['File   fsUploadFile;','static void historiaBladIO()', 'static history193::Engine historiaIo', 'if (!fsUploadFile) return;', 'request->send(200, "text/plain", odp);','request->send(systemFS196,']
refs={n:[i for i,l in enumerate(lines,1) if n in l] for n in needles};assert all(refs.values())
r={'status':'PASS','firmware_sha256':hashlib.sha256(F.read_bytes()).hexdigest(),'firmware_modified':False,'code_locations':refs,'file_fill_method_sha256':hashlib.sha256(body.encode()).hexdigest(),'try_again':constant,'scope':'source audit + one extracted method, not full HTTP transport'}
U=P.parent/'blokada-startu-fs196/upstream'
refs=json.loads((U/'SOURCES.json').read_text())
for name in ['FS.h','FSImpl.h','LittleFS.h']:
 ref=next(v for k,v in refs.items() if k.endswith('/'+name));assert hashlib.sha256((U/name).read_bytes()).hexdigest()==ref['sha256']
t=subprocess.run(['c++','-std=c++11','-fno-exceptions','-fno-rtti','-Wall','-Wextra','-Werror','-fsyntax-only',*['-I'+str(x) for x in [P,P/'host',U]],str(P/'api_syntax.cpp')],capture_output=True,text=True);assert t.returncode==0,t.stderr
r['api_Cxx11_real_LittleFS_header']=True
(P/'AUDYT.json').write_text(json.dumps(r,indent=2)+'\n');print(r)

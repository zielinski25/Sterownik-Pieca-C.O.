from pathlib import Path
import json,hashlib
P=Path(__file__).resolve().parent;R=P.parents[1];M=P.parent/'most-runtime198';s=(M/'Test.cpp').read_text();manifest=json.loads((M/'MANIFEST.json').read_text())['files'];assert hashlib.sha256((M/'Test.cpp').read_bytes()).hexdigest()==manifest[str((M/'Test.cpp').relative_to(R))]
a=s.index('struct Raw:');b=s.index('template<size_t C=2>struct Fixture',a);(P/'Raw198.inc').write_text('// Exact model Calls from 198; not ESP VFS.\n'+s[a:b])
F=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp';assert hashlib.sha256(F.read_bytes()).hexdigest()=='3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31'
s=F.read_text();a=s.index('  wifiWebServer.on("/api/fs-upload"');b=s.index('\n  // [CZ-WIFI-TG.2]',a);(P/'UPLOAD_REFERENCE.txt').write_text(s[a:b]+'\n')
(P/'AUDYT.json').write_text(json.dumps({'firmware_modified':False,'release_ready':False,'source_sha256':hashlib.sha256(F.read_bytes()).hexdigest(),'legacy_upload_issues':['global state shared across requests','index 0 clears previous error','missing File silently skips chunk','final close lock failure not latched','void close cannot certify success'],'scope':'new-file-only offline prototype; original route unchanged'},indent=2)+'\n');print('PASS frozen upload audit and model extraction')

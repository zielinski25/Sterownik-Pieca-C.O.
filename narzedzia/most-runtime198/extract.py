from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;R=P.parents[1]
src=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie/CheckedHistoryIO.h'
s=src.read_text();assert hashlib.sha256(src.read_bytes()).hexdigest()=='8eb1869329308cbec2d09d087e12eb896c3bce59e3b4099d4b295360f1bc78b9'
s=s[s.index('class Engine {'):s.rindex('\n}')]
changes={
 'void(*faultObserver_)();':'void(*faultObserver_)(void*);void*faultContext_;',
 'faultObserver_();':'faultObserver_(faultContext_);',
 'void(*observer)()=nullptr):io_(io),service_(service),faultObserver_(observer){}':'void(*observer)(void*)=nullptr,void*context=nullptr):io_(io),service_(service),faultObserver_(observer),faultContext_(context){}'
}
for a,b in changes.items():assert s.count(a)==1,(a,s.count(a));s=s.replace(a,b)
s='#pragma once\n// Mechanically extracted 193 Engine; only observer context plumbing changed.\n// Cleanup calls are requests: the enclosing gate may suppress actual backend close.\n#include "CheckedHistoryIO.h"\nnamespace history198core {\nusing history193::Calls;using history193::Fault;using history193::Stream;\n'+s+'\n}\n'
(P/'Engine198.h').write_text(s)
(P/'EXTRACTION.json').write_text(json.dumps({'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'result_sha256':hashlib.sha256(s.encode()).hexdigest(),'changes':changes},indent=2)+'\n')
print('PASS exact Engine extraction: 3 observer-context changes only')
modelsrc=R/'narzedzia/uchwyty-runtime197/Test.cpp'
assert hashlib.sha256(modelsrc.read_bytes()).hexdigest()=='e17cb214db4c81b7971b1703ff986cfee18152fd2b1d0c97f8bf9d1abed74f35'
model=modelsrc.read_text().split('// Process-lifetime test fixtures:')[0].replace('events.reserve(10000)','events.reserve(256)')
(P/'Model197.inc').write_text('// Model from 197; event preallocation reduced, no behavior change.\n'+model)

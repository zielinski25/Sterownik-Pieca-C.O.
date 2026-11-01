from pathlib import Path
import tempfile,subprocess,json,hashlib,os,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent
H=N/'uchwyty-runtime197';M=N/'most-runtime198';U=N/'blokada-startu-fs196/upstream';A=N/'odpowiedz-http199'
# Each mutation must compile and must be caught by at least one test group.
mutants=[
 ('unpinned-length','Snapshots.h','s.length = length;','s.length = std::numeric_limits<std::size_t>::max();'),
 ('read-past-end','Snapshots.h','if (s->sent == s->length) return ReadResult(ReadStatus::End);',''),
 ('short-read-is-end','Snapshots.h','if (got == 0 || got > wanted) { release(*s); return ReadResult(ReadStatus::Stopped); }','if (got == 0) { release(*s); return ReadResult(ReadStatus::End); }'),
 ('rotation-ignores-pins','Snapshots.h','if (rotating_ || pinned()) return false;','if (rotating_) return false;'),
 ('open-during-rotation','Snapshots.h','if (!timeout || timeout > 0x7fffffffU || rotating_) return {Code::Unavailable, invalid()};','if (!timeout || timeout > 0x7fffffffU) return {Code::Unavailable, invalid()};'),
 ('no-expiry-on-read','Snapshots.h','if (expired(*s, now)) { release(*s); return ReadResult(ReadStatus::Stale); }',''),
 ('expire-noop','Snapshots.h','if (s.occupied && expired(s, now)) { release(s); ++n; }','if (false) { release(s); ++n; }'),
 ('no-generation-bump','Snapshots.h','++s.generation;',''),
 ('release-keeps-slot','Snapshots.h','s.occupied = false;',''),
 ('stop-reported-as-end','Snapshots.h','if (!owner_.ready() || !source_.ready()) { release(*s); return ReadResult(ReadStatus::Stopped); }','if (!owner_.ready() || !source_.ready()) { release(*s); return ReadResult(ReadStatus::End); }'),
 ('stale-handle-accepted','Snapshots.h','return (s.occupied && s.generation == h.generation) ? &s : nullptr;','return (s.occupied) ? &s : nullptr;'),
 ('zero-read-accepted','Snapshots.h','if (!buffer || !capacity) return ReadResult(ReadStatus::Invalid);',''),
 ('commit-without-file','Snapshots.h','if (s.file.size() < length) { s.file = fs::File(); return {Code::Invalid, invalid()}; }',''),
]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
files=['Test.cpp','Snapshots.h']
original={n:(P/n).read_text() for n in files}
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)>=1,(name,s.count(a));(d/file).write_text(s.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-g','-pthread','-fno-exceptions','-fno-rtti','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',*['-I'+str(x) for x in [d,H,H/'host',H/'upstream',M,U,A]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=120);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
  assert t.returncode!=0,('SURVIVED',name)
  cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'asan_error':'ERROR: AddressSanitizer' in t.stderr})
inputs=[P/n for n in files]+[P/'mutacje.py',H/'RuntimeFilesystem.h',M/'Model197.inc',A/'CheckedFileBody.h',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','profile':'ASan+UBSan, LSan enabled','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n')
print('PASS snapshots207 compiled mutants',len(cases))

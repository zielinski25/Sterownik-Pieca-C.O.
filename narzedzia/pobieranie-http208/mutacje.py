from pathlib import Path
import tempfile,subprocess,json,hashlib,os,resource
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent
H=N/'uchwyty-runtime197';M=N/'most-runtime198';U=N/'blokada-startu-fs196/upstream';A=N/'odpowiedz-http199';D=N/'odpowiedz-odroczona203';S=N/'pobieranie-historia207';C=N/'naglowki-upload206/host'
# Every mutation must compile and must be caught by at least one test group.
mutants=[
 ('no-authorization','DownloadAdapter.h','if (!authorized) return {Code::Forbidden, invalid()};',''),
 ('paused-request-accepted','DownloadAdapter.h','r.isSent() || r.isPaused() || r.getResponse() ||','r.isSent() || r.getResponse() ||'),
 ('install-failure-ignored','DownloadAdapter.h','if (!r.installDeferred203(poll, gone, &s)) {','if (false) {'),
 ('no-generation-bump','DownloadAdapter.h','++s.generation;',''),
 ('release-keeps-pin','DownloadAdapter.h','  closePin(s);\n',''),
 ('finish-ignores-request','DownloadAdapter.h','return (live && live.get() == &r) ? &s : nullptr;','return live ? &s : nullptr;'),
 ('disconnect-keeps-pin','DownloadAdapter.h','  s.disconnected = true;\n  s.owner->release(s);\n','  s.disconnected = true;\n'),
 ('deadline-never-fires','DownloadAdapter.h','if (static_cast<std::uint32_t>(services_.now() - s.started) >= s.timeout) {','if (static_cast<std::uint32_t>(services_.now() - s.started) >= s.timeout && false) {'),
 ('fault-looks-like-end','DownloadAdapter.h','  s.failed = true;  // Stopped/Stale/Invalid must never look like a clean end\n  return 0;','  return 0;'),
 ('valid-ignores-failure','DownloadAdapter.h','return s.occupied && !s.failed && !s.disconnected && s.snap.generation && !s.done;','return s.occupied && !s.disconnected && s.snap.generation && !s.done;'),
 ('length-off-by-one','DownloadAdapter.h','const std::size_t length = snapshots_.remaining(opened.handle);','const std::size_t length = snapshots_.remaining(opened.handle) + 1;'),
 ('oom-keeps-pin','DownloadAdapter.h','if (!response) { snapshots_.close(opened.handle); return {Code::Unavailable, invalid()}; }','if (!response) { return {Code::Unavailable, invalid()}; }'),
 ('no-content-length','SnapshotResponse.h','_sendContentLength = true;','_sendContentLength = false;'),
 ('source-always-valid','SnapshotResponse.h','bool _sourceValid() const override { return reader_.valid(); }','bool _sourceValid() const override { return true; }'),
]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
files=['Test.cpp','SnapshotResponse.h','DownloadAdapter.h','Fixture208.inc','Transport208.inc','ResponseTail208.inc','Abstract.inc','CheckedFileBody.inc']
original={n:(P/n).read_text() for n in files}
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)==1,(name,file,s.count(a));(d/file).write_text(s.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-g','-pthread','-fno-exceptions','-fno-rtti','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
       '-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT=1',*['-I'+str(x) for x in [d,C,H,S,H/'upstream',M,U,A,D]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=180);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=90,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'))
  assert t.returncode!=0,('SURVIVED',name)
  cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'asan_error':'ERROR: AddressSanitizer' in t.stderr})
inputs=[P/n for n in files]+[P/'mutacje.py',S/'Snapshots.h',H/'RuntimeFilesystem.h',M/'Model197.inc',A/'CheckedFileBody.h',D/'Methods.inc',U/'FS.cpp']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','profile':'ASan+UBSan, LSan enabled, chunk-inflight on','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n')
print('PASS download208 compiled mutants',len(cases))

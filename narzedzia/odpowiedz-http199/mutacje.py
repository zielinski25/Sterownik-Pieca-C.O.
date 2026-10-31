from pathlib import Path
import subprocess,tempfile,json,hashlib,resource
P=Path(__file__).resolve().parent;R=P.parents[1];H=P.parent/'uchwyty-runtime197';M=P.parent/'most-runtime198';U=P.parent/'blokada-startu-fs196/upstream'
mutants=[
 ('plain-missing-both-late-source-checks','Transport.inc','!_sourceValid() || (readLen != RESPONSE_TRY_AGAIN && readLen > maxLen)','(readLen != RESPONSE_TRY_AGAIN && readLen > maxLen)'),
 ('plain-no-count-bound','Transport.inc','readLen > maxLen','false'),
 ('chunk-no-count-bound','Transport.inc','readLen > std::min(_send_buffer->size(), tcp_win) - 8','false'),
 ('premature-final-completion','Abstract.inc','(_state > RESPONSE_WAIT_ACK && _send_buffer_len == 0)','(_state > RESPONSE_WAIT_ACK)'),
 ('never-drain-final-buffer','Transport.inc','_state == RESPONSE_CONTENT || (_state == RESPONSE_END && _send_buffer_len)','_state == RESPONSE_CONTENT'),
 ('missing-pre-send-source','Transport.inc','''    if (!_sourceValid()) {
        _state = RESPONSE_FAILED;
        request->client()->close();
        return 0;
      }
    // execute''','    // execute'),
 ('ignore-owner-stop','CheckedFileBody.h','return !failed_&&owner_.ready();','return !failed_;'),
 ('ignore-short-file','CheckedFileBody.h','||got==0',''),
 ('ignore-impossible-read','CheckedFileBody.h','||got>wanted',''),
 ('lost-failure-latch','CheckedFileBody.h','if(!owner_.ready()||got==0||got>wanted){failed_=true;return 0;}','if(!owner_.ready()||got==0||got>wanted){return 0;}'),
 ('no-remaining-progress','CheckedFileBody.h','remaining_-=got;',''),
 ('wrong-advertised-length','CheckedFileResponse.h','_contentLength=expected;','_contentLength=expected+1;'),
]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
with tempfile.TemporaryDirectory() as td:
 d=Path(td);names=['Test.cpp','Abstract.inc','Transport.inc','CheckedFileBody.h','CheckedFileResponse.h'];original={n:(P/n).read_text() for n in names}
 for name,file,a,b in mutants:
  for n,s in original.items():(d/n).write_text(s)
  s=original[file];assert s.count(a)==1,(name,s.count(a));s=s.replace(a,b)
  if name=='plain-missing-both-late-source-checks':
   extra=mutants[5][2];assert s.count(extra)==1;s=s.replace(extra,mutants[5][3])
  (d/file).write_text(s)
  cmd=['c++','-std=c++11','-O1','-pthread','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in [d,P/'host',H,M,U]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=20);assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode,'edits':2 if name=='plain-missing-both-late-source-checks' else 1})
inputs=[P/n for n in names]+[P/'mutacje.py',P/'host/Arduino.h',H/'RuntimeFilesystem.h',M/'Model197.inc',U/'FS.cpp',U/'FS.h',U/'FSImpl.h']
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},'cases':cases},indent=2)+'\n');print('PASS compiled http199 mutants',len(cases))

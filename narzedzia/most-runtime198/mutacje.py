from pathlib import Path
import tempfile,subprocess,json,hashlib,resource
P=Path(__file__).resolve().parent;R=P.parents[1];H=P.parent/'uchwyty-runtime197';U=P.parent/'blokada-startu-fs196/upstream';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie';original=(P/'RuntimeHistory.h').read_text()
lease='typename Owner::Lease lease(owner_);if(!lease)return result(false,false);'
mutants=[
 ('missing-append-transaction-lock',lease+'\n  const bool confirmed=engine_.append','if(!owner_.ready())return result(false,false);\n  const bool confirmed=engine_.append'),
 ('missing-trim-transaction-lock',lease+'\n  const bool confirmed=engine_.trim','if(!owner_.ready())return result(false,false);\n  const bool confirmed=engine_.trim'),
 ('close-after-stop','int close(int fd)override{\n   if(!permitted(fd))return -1;','int close(int fd)override{\n   if(!find(fd))return -1;'),
 ('repeat-uncertain-close','const int r=raw_.close(fd);const int e=raw_.error();','const int r=raw_.close(fd);const int e=raw_.error();if(r!=0)raw_.close(fd);'),
 ('lose-positive-open-error-handle','slot->fd=fd; // retain','slot->fd=fd;if(e)slot->state=Free; // retain'),
 ('lose-uncertain-close-slot','slot->state=CloseAttempted; // never retry','slot->state=Free; // never retry'),
 ('semantic-fault-does-not-stop-owner','self.owner_.trip();',''),
 ('omit-application-notification','if(self.observer_)self.observer_(self.context_);',''),
 ('open-before-reservation','if(!slot)return poison(EMFILE);','if(!slot){raw_.open(p,flags);return poison(EMFILE);}'),
 ('hide-error-number','int error()const override{return error_;}','int error()const override{return 0;}'),
 ('false-confirmation','return result(true,confirmed);\n }\n Result trim','return result(true,true);\n }\n Result trim'),
 ('retract-confirmed-after-later-stop','return result(true,confirmed);\n }\n Result trim','return result(true,owner_.ready()&&confirmed);\n }\n Result trim'),
 ('ignore-stop-during-close','if(r!=0||e||!owner_.ready())return poison(e);\n   slot->state=Free;','if(r!=0||e)return poison(e);\n   slot->state=Free;'),
 ('skip-healthy-close','const int r=raw_.close(fd);const int e=raw_.error();','const int r=0;const int e=0;'),
]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
with tempfile.TemporaryDirectory() as td:
 d=Path(td)
 for f in ['Test.cpp','Engine198.h','Model197.inc']:(d/f).write_bytes((P/f).read_bytes())
 for name,a,b in mutants:
  assert original.count(a)==1,(name,original.count(a));(d/'RuntimeHistory.h').write_text(original.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-pthread','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in [d,H,H/'host',U,B]],d/'Test.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=20);assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode})
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [P/'RuntimeHistory.h',P/'Engine198.h',P/'Model197.inc',P/'Test.cpp',P/'mutacje.py',H/'RuntimeFilesystem.h',H/'host/Arduino.h',U/'FS.cpp',U/'FS.h',U/'FSImpl.h',B/'CheckedHistoryIO.h']},'cases':cases},indent=2)+'\n');print('PASS compiled runtime198 mutants',len(cases))

from pathlib import Path
import tempfile,subprocess,shutil,json,hashlib,resource
P=Path(__file__).resolve().parent;U=P.parent/'blokada-startu-fs196/upstream';original=(P/'RuntimeFilesystem.h').read_text()
mutants=[
 ('admit-after-stop','allowed_=o_.ready();','allowed_=true;'),
 ('omit-stop-publication','state_.store(State::Stopped,std::memory_order_release);',''),
 ('allow-reenable','State old=State::Cold;return state_.compare_exchange_strong(old,State::Ready,std::memory_order_release);','state_.store(State::Ready);return true;'),
 ('omit-last-reference-close','if(armed)o_.release(i_);',''),
 ('close-after-stop','if(!l){s.state=Held;return;}',''),
 ('drop-close-shadow','s.shadow=s.file;s.file.close();','s.file.close();'),
 ('reuse-live-control-block','&&!slots_[i].block.busy.load(std::memory_order_acquire)',''),
 ('ignore-short-write','if(done!=n)trip(Fault::ShortWrite);',''),
 ('ignore-invalid-read-count','if(got>n)trip(Fault::ReadCount);',''),
 ('escape-raw-FS','fs::FS&fs(){return view_;}','fs::FS&fs(){return backend_;}'),
 ('drain-without-owner','mutex_.lock();bool stopped=!ready();mutex_.unlock();return stopped;','return !ready();'),
 ('ack-write-after-stop','return ready()?done:0;','return done;'),
 ('ignore-input-path-bound','if(n==PathBytes)return {};',''),
 ('lose-child-path','if(!copyPath(s,p))','if(false)')]
resource.setrlimit(resource.RLIMIT_CORE,(0,0));cases=[]
with tempfile.TemporaryDirectory() as td:
 d=Path(td);shutil.copytree(P/'host',d/'host')
 for f in ['Test.cpp','Allocations.cpp','CheckedRead.h','WebFill.inc']:(d/f).write_bytes((P/f).read_bytes())
 for name,a,b in mutants:
  assert original.count(a)==1,(name,original.count(a));(d/'RuntimeFilesystem.h').write_text(original.replace(a,b))
  cmd=['c++','-std=c++11','-O1','-pthread','-fno-exceptions','-fno-rtti',*['-I'+str(x) for x in [d,d/'host',U]],d/'Test.cpp',d/'Allocations.cpp',U/'FS.cpp','-o',d/'test']
  t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90);assert t.returncode==0,(name,t.stderr)
  t=subprocess.run([d/'test'],capture_output=True,text=True,timeout=15);assert t.returncode!=0,('SURVIVED',name);cases.append({'name':name,'compiled':True,'detected':True,'returncode':t.returncode})
(P/'MUTACJE.json').write_text(json.dumps({'status':'PASS','inputs':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [P/'RuntimeFilesystem.h',P/'CheckedRead.h',P/'Test.cpp',P/'Allocations.cpp',P/'mutacje.py']},'cases':cases},indent=2)+'\n');print('PASS compiled runtime197 mutants',len(cases))

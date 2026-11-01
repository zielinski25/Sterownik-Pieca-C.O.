from pathlib import Path
import subprocess,tempfile,hashlib,json,os
P=Path(__file__).resolve().parent;R=P.parents[1];N=P.parent;U=N/'blokada-startu-fs196/upstream';H=N/'uchwyty-runtime197';B=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie';C=N/'littlefs-1.20.4-133/zrodla'
subprocess.run(['python3','-B',str(P/'extract.py')],check=True)
source=json.loads((U/'SOURCES.json').read_text())
for name in ['FS.cpp','FS.h','FSImpl.h']:
 ref=next(v for k,v in source.items() if k.endswith('/'+name));assert hashlib.sha256((U/name).read_bytes()).hexdigest()==ref['sha256']
refs=json.loads((C.parent/'ZRODLA.json').read_text())
for name in ['lfs.c','lfs.h','lfs_util.c','lfs_util.h']:assert hashlib.sha256((C/name).read_bytes()).hexdigest()==refs['files'][name]['sha256']
base=P/'WYNIKI';base.mkdir(exist_ok=True);i=1
while (base/f'run{i:03d}').exists():i+=1
out=base/f'run{i:03d}';out.mkdir()
inputs=[*P.glob('*.h'),*P.glob('*.cpp'),*P.glob('*.py'),*P.glob('*.inc'),*H.glob('*.h'),H/'Test.cpp',*H.glob('host/*'),U/'SOURCES.json',U/'FS.cpp',U/'FS.h',U/'FSImpl.h',B/'CheckedHistoryIO.h',C.parent/'ZRODLA.json',*[C/n for n in ['lfs.c','lfs.h','lfs_util.c','lfs_util.h']]]
r={'status':'RUNNING','release_ready':False,'firmware_modified':False,'inputs':{str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs if p.is_file()},'profiles':{},'compiler':subprocess.check_output(['c++','--version'],text=True).splitlines()[0]}
def save():(out/'wynik.json').write_text(json.dumps(r,indent=2)+'\n')
def run(cmd):
 t=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=90,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1'));assert t.returncode==0,(cmd,t.stdout,t.stderr);return t.stdout
try:
 with tempfile.TemporaryDirectory() as td:
  d=Path(td)
  for profile,flags in [('native',[]),('asan-ubsan',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'])]:
   opts=['-O1','-g','-Wall','-Wextra','-Werror',*flags];inc=['-I'+str(x) for x in [P,H,H/'host',U,B,C]];cpp=['c++','-std=c++11','-pthread','-fno-exceptions','-fno-rtti',*opts,*inc]
   run([*cpp,P/'Test.cpp',U/'FS.cpp','-o',d/'test']);r['profiles'][profile+'-model']=run([d/'test']);print(r['profiles'][profile+'-model']);save()
   defs=['-DLFS_NO_DEBUG','-DLFS_NO_WARN','-DLFS_NO_ERROR']
   for name in ['lfs','lfs_util']:run(['cc','-std=c99',*opts,*inc,*defs,'-c',C/(name+'.c'),'-o',d/(name+'.o')])
   run([*cpp,*defs,P/'CoreTest.cpp',U/'FS.cpp',d/'lfs.o',d/'lfs_util.o','-o',d/'core']);r['profiles'][profile+'-core']=run([d/'core']);print(r['profiles'][profile+'-core']);save()
 r['status']='PASS';save();print(out)
except Exception as e:r['status']='FAIL';r['error']=str(e);save();raise

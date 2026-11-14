import pathlib,subprocess,sys,tempfile
s=pathlib.Path(sys.argv[1]).read_text();a=s.index('void historiaDlugaAkumulujProbke(const DateTime &now',s.index('static HistoriaDlugaOstatnia historiaDlugaOstatnia;'));b=s.index('// [HISTORIA-DLUGA, Partia 3',a);fn=s[a:b]
a=s.index('struct HistoriaDlugaOstatnia {');b=s.index('static HistoriaDlugaOstatnia historiaDlugaOstatnia;',a); struct=s[a:b]
pre=r'''
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include "HistoriaExtra.h"
#define HIST_DLUGA_MIN_ODSTEP_MS 30000UL
#define HIST_DLUGA_HEARTBEAT_MS 3600000UL
#define HIST_PROG_TEMP 1.0f
#define HIST_PROG_CISNIENIE 0.05f
#define HIST_PROG_WILGOTNOSC 3.0f
#define HIST_PROG_ANGLE 5.0f
struct DateTime{}; int *historiaDlugaBufor=nullptr; bool g_rtcGotowy=true,g_rtcNtpZsynchronizowany=false,pump=false;uint32_t t=0;uint32_t millis(){return t;}bool pompaJestWlaczona(){return pump;}
'''
mid=r'''
HistoriaDlugaOstatnia historiaDlugaOstatnia; int saved=0;
void historiaDlugaZapiszProbke(const DateTime&,float a,float b,float c,float d,float e,float f,float g,float h,float i,int j,int k,bool p,const HistoriaExtra &x){
 ++saved;auto &z=historiaDlugaOstatnia;z.mamy=true;z.czasMs=t;z.t_zewn=a;z.t_bojler=b;z.t_ogrz=c;z.t_ogrz_sr=d;z.t_powrot=e;z.t_panel=f;z.t_pokoj=g;z.cisnienie=h;z.wilgotnosc=i;z.klapa_proc=j;z.syberko_proc=k;z.pompa=p;z.extra=x;
}
'''
main=r'''
int main(){int buf=1;historiaDlugaBufor=&buf;HistoriaExtra x=historiaExtraSnapshot(true,0,0,2047,100,100,true);DateTime now;
 auto go=[&](){historiaDlugaAkumulujProbke(now,1,1,1,1,1,1,1,1000,50,90,40,x);};
 g_rtcGotowy=false;go();assert(saved==0);g_rtcGotowy=true;go();assert(saved==1);
 x.simulated=128;t=1000;go();assert(saved==1);t=60000;go();assert(saved==2);
 t+=60000;x.trocinyX10+=4;go();assert(saved==2);x.trocinyX10++;go();assert(saved==3);
 t+=60000;x.dym+=49;go();assert(saved==3);x.dym++;go();assert(saved==4);
 t+=60000;x.known &= ~1024;x.simulated &= ~1024;x.dym=-1;go();assert(saved==5);
 t+=3600000;go();assert(saved==6);pump=true;t+=60000;go();assert(saved==7);
}
'''
with tempfile.TemporaryDirectory() as d:
 f=pathlib.Path(d)/'a.cpp';f.write_text(pre+struct+mid+fn+main)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(pathlib.Path(sys.argv[1]).parent),str(f),'-o',d+'/x'],check=True)
 subprocess.run([d+'/x'],check=True)
print('OK: produkcyjne progi zdarzen, symulacja, odstep i heartbeat')

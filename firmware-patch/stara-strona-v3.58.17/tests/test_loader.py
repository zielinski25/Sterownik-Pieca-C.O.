# Compile the actual v17 loaders and serializers with a tiny in-memory Arduino/FS shim.
import pathlib, subprocess, sys, tempfile
src=pathlib.Path(sys.argv[1]).read_text()
def between(a,b):
 i=src.index(a);j=src.index(b,i);return src[i:j]
shortstruct=between('struct HistoriaProbka {','\n\nHistoriaProbka* historiaBufor')
longstruct=between('struct HistoriaProbkaDluga {','\n\nHistoriaProbkaDluga* historiaDlugaBufor')
shortfn=between('void historiaLiniaCsv(const HistoriaProbka &p, String &csv) {','\n\nString historiaZbudujCsv()')
longfn=between('void historiaDlugaLiniaCsv(const HistoriaProbkaDluga &p, String &csv) {','\n\nString historiaDlugaZbudujCsv()')
loaders=between('void historiaWczytajZFS() {','// [v3.55.3 HISTORIA-STREAM-END]')
preamble=r'''
#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include "HistoriaExtra.h"
struct String:std::string {
 using std::string::string; String()=default; String(const std::string& x):std::string(x) {}
 String(float v,int n){char b[50];snprintf(b,sizeof b,"%.*f",n,(double)v);append(b);}
 String(int v){append(std::to_string(v));} String(uint32_t v){append(std::to_string(v));}
 String substring(unsigned a,unsigned b)const{return substr(a,b-a);}
 float toFloat()const{return strtof(c_str(),nullptr);} long toInt()const{return strtol(c_str(),nullptr,10);}
};
struct File { std::string data; bool ok=false; explicit operator bool()const{return ok;} void close(){} };
struct FS {std::string sh,lo; bool exists(const char*){return true;} File open(const char *p,const char*){File f;f.data=strcmp(p,"s")==0?sh:lo;f.ok=true;return f;}} systemFS196;
struct HistoriaStrumien {std::vector<std::string> v;unsigned idx=0;bool otworz(File &f,unsigned,unsigned){std::istringstream in(f.data);std::string x;while(std::getline(in,x))v.push_back(x);return true;} bool nastepna(String &out){if(idx==v.size())return false;out=v[idx++];return true;}void zamknij(){} unsigned linie(){return idx;}unsigned bajty(){return 0;}unsigned rozmiar(){return 0;}unsigned czasMs(){return 0;}bool przerwano(){return false;}};
#define HISTORIA_MAX_PROBEK 20
#define HISTORIA_DLUGA_MAX_PROBEK 20
#define HISTORIA_PLIK "s"
#define HISTORIA_DLUGA_PLIK "l"
#define HISTORIA_WCZYTAJ_LIMIT_BAJTOW 1
#define HISTORIA_WCZYTAJ_LIMIT_MS 1
#define HISTORIA_DLUGA_WCZYTAJ_LIMIT_BAJTOW 1
#define HISTORIA_DLUGA_WCZYTAJ_LIMIT_MS 1
#define DLOG(...) do{}while(0)
#define DLOGF(...) do{}while(0)
#define F(x) x
bool littlefsReady=true; bool historiaDostepGotowy(){return true;}
uint32_t historiaNormalizujLegacyUnix(const char*,uint32_t t,bool){return t;}
'''
globals_=r'''
HistoriaProbka sbuf[20],*historiaBufor=sbuf; HistoriaProbkaDluga lbuf[20],*historiaDlugaBufor=lbuf;
int historiaGlowa=0,historiaLiczbaProbek=0,historiaPlikLiczbaLinii=0,historiaDlugaGlowa=0,historiaDlugaLiczbaProbek=0,historiaDlugaPlikLiczbaLinii=0;
bool historiaPelna=false,historiaDlugaPelna=false;
'''
main=r'''
int main(){
 std::string s="12:34,1,2,3,4,5,6,7,1,1000,40,90,40,1700000000";
 systemFS196.sh=s+"\n"+s+",-0.1,0,1152,1024\n"+s+",,0,1152,0\n"+s+",12,4\n";
 historiaWczytajZFS();assert(historiaLiczbaProbek==3);assert(sbuf[0].extra.trocinyX10==INT16_MIN&&sbuf[0].extra.dym==-1&&sbuf[0].extra.known==0);
 assert(sbuf[1].extra.trocinyX10==-1&&sbuf[1].extra.dym==0&&sbuf[1].extra.simulated==1024);
 assert(sbuf[2].extra.known==0&&sbuf[2].extra.dym==-1&&sbuf[2].extra.trocinyX10==INT16_MIN);
 String out;historiaLiniaCsv(sbuf[1],out);assert(out.find(",-0.1,0,1152,1024\n")!=std::string::npos);
 std::string l="01.01 12:34";for(int i=0;i<33;i++)l+=",1";l+=",1700000000";
 systemFS196.lo=l+"\n"+l+",1\n"+l+",0,0.0,4095,1152,128\n"+l+",0,1,1,1152,2048\n"+l+",1,2\n";
 historiaDlugaWczytajZFS();assert(historiaDlugaLiczbaProbek==4);assert(lbuf[0].pompa==-1&&lbuf[0].extra.known==0&&lbuf[0].extra.dym==-1);
 assert(lbuf[1].pompa==1&&lbuf[1].extra.trocinyX10==INT16_MIN);assert(lbuf[2].pompa==0&&lbuf[2].extra.trocinyX10==0&&lbuf[2].extra.dym==4095&&lbuf[2].extra.simulated==128);
 assert(lbuf[3].extra.known==0&&lbuf[3].extra.dym==-1);String out2;historiaDlugaLiniaCsv(lbuf[2],out2);assert(out2.find(",0,0.0,4095,1152,128\n")!=std::string::npos);
}
'''
with tempfile.TemporaryDirectory() as d:
 file=pathlib.Path(d)/'test.cpp';file.write_text(preamble+shortstruct+longstruct+globals_+shortfn+longfn+loaders+main)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-I'+str(pathlib.Path(sys.argv[1]).parent),str(file),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
print('OK: produkcyjne czytniki i serializatory, stare/nowe/uszkodzone wiersze i round-trip')

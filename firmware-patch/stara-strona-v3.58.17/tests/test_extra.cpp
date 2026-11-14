#include <cassert>
#include <cstring>
#include <type_traits>
#include "HistoriaExtra.h"
int main(){
 static_assert(sizeof(HistoriaExtra)==8,"memory budget");
 static_assert(std::is_trivial<HistoriaExtra>::value,"PSRAM malloc and queue memcpy require trivial type");
 char b[48]; HistoriaExtra e=historiaExtraBrak(), f=historiaExtraBrak();
 historiaExtraFormat(e,b,sizeof b); assert(!strcmp(b,",,,0,0"));
 assert(historiaExtraParsuj("","","0","0",f)); assert(f.trocinyX10==INT16_MIN&&f.dym==-1&&f.known==0);
 // All good and per-channel flags, including temperature zero and negative.
 e=historiaExtraSnapshot(true,0,0x481,2047,-10,4095,true);assert(e.known==2047&&e.simulated==0x481&&e.dym==4095&&e.trocinyX10==-10);
 historiaExtraFormat(e,b,sizeof b);assert(!strcmp(b,",-1.0,4095,2047,1153"));
 assert(historiaExtraParsuj("-1.0","4095","2047","1153",f));assert(!memcmp(&e,&f,sizeof e));
 e=historiaExtraSnapshot(true,(1U<<14)|(2U<<20),2047,2047,100,42,true); assert(!(e.known&128)&&!(e.known&1024));assert(e.trocinyX10==INT16_MIN&&e.dym==-1); assert((e.simulated&~e.known)==0);
 e=historiaExtraSnapshot(false,0,2047,2047,100,1,true);assert(e.known==0&&e.trocinyX10==INT16_MIN&&e.dym==-1);
 e=historiaExtraSnapshot(true,0,2047,2047,-301,4096,true);assert(!(e.known&1152));
 f=historiaExtraBrak();assert(!historiaExtraParsuj("0","0","0","0",f));assert(!historiaExtraParsuj("","","128","0",f));
 assert(!historiaExtraParsuj("","","0","1",f));assert(!historiaExtraParsuj("1","0","1152","2048",f));assert(!historiaExtraParsuj("nan","0","1152","0",f));
 assert(!historiaExtraParsuj("0","1oops","1152","0",f));assert(!historiaExtraParsuj("0","-1","1152","0",f));assert(!historiaExtraParsuj("0","1","+1152","0",f));
 assert(f.known==0&&f.dym==-1&&f.trocinyX10==INT16_MIN);
}

#include "InternetProbe.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace InternetProbe;
struct FakeUdp {
  bool startOk = true, answer = false;
  int starts = 0, stops = 0, server = -1;
  uint32_t address = 0; uint16_t id = 0;
  bool start(uint32_t ip, int s, uint16_t token) { ++starts; address = ip; server = s; id = token; return startOk; }
  bool odebrano(uint16_t token) { assert(token == id); bool a = answer; answer = false; return a; }
  void stop() { ++stops; }
};
struct Publish {
  int* val; uint32_t* time; int* calls;
  void operator()(int v, uint32_t t) { *val = v; *time = t; ++*calls; }
};
int main() {
  // Parser otrzymuje pelny DNS A z echem pytania; odrzuca falszywe/uciete.
  std::vector<uint8_t> reply(QUERY, QUERY + sizeof(QUERY));
  reply[0]=0x12; reply[1]=0x34; reply[2]=0x81; reply[3]=0x80; reply[7]=1;
  const uint8_t rr[] = {0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,1,2,3,4};
  reply.insert(reply.end(), rr, rr + sizeof(rr));
  assert(poprawnaOdpowiedz(reply.data(), reply.size(), 0x1234));
  assert(!poprawnaOdpowiedz(reply.data(), reply.size(), 0x9999));
  assert(!poprawnaOdpowiedz(reply.data(), reply.size()-1, 0x1234));
  auto b = reply; b[2] |= 2; assert(!poprawnaOdpowiedz(b.data(),b.size(),0x1234));
  b = reply; b[3] |= 3; assert(!poprawnaOdpowiedz(b.data(),b.size(),0x1234));
  b = reply; b[7] = 0; assert(!poprawnaOdpowiedz(b.data(),b.size(),0x1234));
  b = reply; b[13] = 'X'; assert(!poprawnaOdpowiedz(b.data(),b.size(),0x1234));
  b = reply; b[sizeof(QUERY)+3] = 5; assert(!poprawnaOdpowiedz(b.data(),b.size(),0x1234));
  int result=-1, calls=0; uint32_t time=0;
  FakeUdp udp; Monitor<FakeUdp, Publish> mon(udp, Publish{&result,&time,&calls});
  mon.tick(0,false,0,1); assert(udp.starts == 0);
  mon.tick(100,true,42,1); assert(udp.starts == 1 && udp.server == 0 && result == -1);
  mon.tick(2099,true,42,2); assert(udp.starts == 1);
  mon.tick(2100,true,42,2); assert(udp.starts == 2 && udp.server == 1 && result == -1);
  mon.tick(4100,true,42,3); assert(result == 0 && time == 4100);
  mon.tick(19099,true,42,3); assert(udp.starts == 2);
  mon.tick(19100,true,42,4); assert(udp.starts == 3 && udp.server == 0);
  udp.answer=true; mon.tick(19200,true,42,5); assert(result == 1 && time == 19200);
  assert(widok(true,19201,result,time)==1);
  assert(widok(true,19200+MAX_WIEK_MS+1,result,time)==-1);
  assert(widok(false,19201,result,time)==0);
  // Zmiana adresu i zanik polaczenia uniewazniaja stary wynik.
  mon.tick(19300,true,43,6); assert(result == -1 && udp.starts == 4 && udp.address == 43);
  mon.tick(19400,false,0,7); assert(result == -1);
  mon.tick(19500,true,43,8); assert(result == -1 && udp.starts == 5);
  udp.startOk=false;
  mon.tick(21500,true,43,9); assert(result == -1); // lokalny blad != brak WAN
  // Liczenie wieku i timeoutu przy przepelnieniu 32-bitowego millis().
  assert(widok(true,20,1,0xfffffff0u)==1);
  FakeUdp wrap; int v=0, c=0; uint32_t t=0;
  Monitor<FakeUdp, Publish> m2(wrap, Publish{&v,&t,&c});
  m2.tick(0xffffff00u,true,5,1); m2.tick(0xffffff00u+TIMEOUT_MS,true,5,2);
  assert(wrap.server == 1);
  m2.tick(0xffffff00u+2*TIMEOUT_MS,true,5,3); assert(v == 0);
  puts("OK centrala: DNS, time-out, retry, rozlaczenie, zmiana IP i millis wrap");
}

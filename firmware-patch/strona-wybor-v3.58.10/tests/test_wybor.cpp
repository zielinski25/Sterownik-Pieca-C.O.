#include "../centrala/src/WyborStrony.h"
#include <cassert>
int main() {
  int writes=0; uint8_t durable=0;
  auto zapisz=[&](uint8_t v) { ++writes; durable=v; return true; };
  WyborStrony v;
  assert(v.odczytaj()==0);
  assert(v.ustaw(0,zapisz) && writes==0);
  assert(v.ustaw(1,zapisz) && writes==1 && durable==1 && v.odczytaj()==1);
  assert(v.ustaw(1,zapisz) && writes==1);
  assert(!v.ustaw(2,zapisz) && writes==1 && v.odczytaj()==1);
  assert(!v.ustaw(0,[](uint8_t){return false;}) && v.odczytaj()==1);
  WyborStrony poRestarcie; poRestarcie.przywroc(durable); assert(poRestarcie.odczytaj()==1);
  assert(poRestarcie.ustaw(0,zapisz) && writes==2 && durable==0);
  poRestarcie.przywroc(255); assert(poRestarcie.odczytaj()==0);
}

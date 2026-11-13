#include "../lcd/src/PogodaWybor.h"
#include <cassert>
using namespace PogodaIkony;
int main() {
  assert(wybierz(-1,-1)==Nieznana && wybierz(0,-1)==Nieznana && wybierz(100,1)==Nieznana);
  assert(wybierz(0,1)==Slonce && wybierz(0,0)==Ksiezyc);
  assert(wybierz(1,1)==SlonceChmura && wybierz(2,0)==KsiezycChmura);
  assert(wybierz(3,1)==Chmura && wybierz(45,0)==Mgla && wybierz(48,1)==Mgla);
  for(int k=51;k<=67;k++) assert(wybierz(k,1)==Deszcz);
  for(int k=80;k<=82;k++) assert(wybierz(k,0)==Deszcz);
  for(int k=71;k<=77;k++) assert(wybierz(k,1)==Snieg);
  for(int k=85;k<=86;k++) assert(wybierz(k,0)==Snieg);
  for(int k=95;k<=99;k++) assert(wybierz(k,1)==Burza);
  assert(wybierz(4,1)==SlonceChmura && wybierz(4,0)==KsiezycChmura);
}

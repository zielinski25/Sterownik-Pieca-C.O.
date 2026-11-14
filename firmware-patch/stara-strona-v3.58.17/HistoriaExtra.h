#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>

// v3.58.17: dolaczone pola CSV: trociny °C, dym ADC, known mask, simulated mask.
// Bity wg TelemetryChannel 0..10. Znane pochodzenie tylko dla swiezej,
// poprawnej probki; stara/niekompletna metadana nie jest automatycznie realna.
struct HistoriaExtra {
  int16_t trocinyX10;
  int16_t dym;
  uint16_t known;
  uint16_t simulated;
};
static constexpr uint16_t HIST_EXTRA_MASK = 0x07ffU;
static constexpr uint16_t HIST_EXTRA_TROCINY = 1U << 7;
static constexpr uint16_t HIST_EXTRA_DYM = 1U << 10;

inline HistoriaExtra historiaExtraBrak() { return HistoriaExtra{INT16_MIN,-1,0,0}; }
inline bool historiaExtraDigits(const char *s) { if (!s || !*s) return false; for (;*s;s++) if (*s < '0' || *s > '9') return false; return true; }

inline HistoriaExtra historiaExtraSnapshot(bool built, uint32_t quality, uint16_t sim, uint16_t eligible, int16_t trociny, uint16_t dym, bool dymSwiezy) {
  HistoriaExtra e=historiaExtraBrak();
  if (!built) return e;
  for (unsigned i=0;i<11;i++) if ((eligible & (1U << i)) && ((quality >> (2U*i)) & 3U)==0U) e.known |= (uint16_t)(1U<<i);
  e.simulated = sim & e.known & HIST_EXTRA_MASK;
  if ((e.known & HIST_EXTRA_TROCINY) && trociny >= -300 && trociny <= 1200) e.trocinyX10=trociny;
  else {e.known &= (uint16_t)~HIST_EXTRA_TROCINY; e.simulated &= (uint16_t)~HIST_EXTRA_TROCINY;}
  if (dymSwiezy && (e.known & HIST_EXTRA_DYM) && dym<=4095) e.dym=(int16_t)dym;
  else {e.known &= (uint16_t)~HIST_EXTRA_DYM; e.simulated &= (uint16_t)~HIST_EXTRA_DYM;}
  return e;
}
// Wiersz ma dokladnie cztery pola rozszerzenia. Uszkodzone pola NIE moga
// generowac falszywych wartosci czy pochodzenia. Zwraca false przy bledzie.
inline bool historiaExtraParsuj(const char* t, const char* d, const char* k, const char* s, HistoriaExtra &out) {
  HistoriaExtra tmp=historiaExtraBrak(); char *end=nullptr;
  if (!t || !d || !k || !s) return false;
  if (!historiaExtraDigits(k) || !historiaExtraDigits(s)) return false;
  unsigned long known=strtoul(k,&end,10); if (*end || known>HIST_EXTRA_MASK) return false;
  unsigned long sim=strtoul(s,&end,10); if (*end || sim>HIST_EXTRA_MASK || (sim & ~known)) return false;
  tmp.known=(uint16_t)known; tmp.simulated=(uint16_t)sim;
  if (*t) { float f=strtof(t,&end); if (*end || !isfinite(f) || f < -30.0f || f > 120.0f || !(known & HIST_EXTRA_TROCINY)) return false; tmp.trocinyX10=(int16_t)lroundf(f*10.0f); }
  else if (known & HIST_EXTRA_TROCINY) return false;
  if (*d) { if (!historiaExtraDigits(d)) return false; long v=strtol(d,&end,10); if (*end || v<0 || v>4095 || !(known & HIST_EXTRA_DYM)) return false; tmp.dym=(int16_t)v; }
  else if (known & HIST_EXTRA_DYM) return false;
  out=tmp; return true;
}
inline void historiaExtraFormat(const HistoriaExtra &e, char *buf, size_t size) {
  char t[16]="", d[12]="";
  if (e.trocinyX10 != INT16_MIN) {
    int v=e.trocinyX10, av=v<0 ? -v : v;
    snprintf(t,sizeof(t),"%s%d.%d",v<0?"-":"",av/10,av%10);
  }
  if (e.dym>=0) snprintf(d,sizeof(d),"%d",(int)e.dym);
  snprintf(buf,size,",%s,%s,%u,%u",t,d,(unsigned)e.known,(unsigned)e.simulated);
}

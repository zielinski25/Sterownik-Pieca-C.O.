// Test na PC telemetrii minutowej z main_centrala.cpp v3.58.22: pola "dht_kod" / "dht_surowa"
// w rekordzie Firebase, maskowanie "sim" do kanalow 0..10, stare probki bez tych pol.
// Kompilacja: g++ -std=c++17 -Wall -Wextra -Werror test_telemetria_dht.cpp (po wytnij_telemetria.py)
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <cstdarg>
#include <cmath>
#include <ctime>
#include <string>
static int sprawdzen = 0, bledy = 0;
#define SPRAWDZ(w, opis) do { ++sprawdzen; if (!(w)) { ++bledy; std::printf("  BLAD [%d] %s\n", __LINE__, opis); } } while (0)
static float g_tempDiag = NAN;
static float czytajTempDiag() { return g_tempDiag; }
using std::isfinite; // Arduino ma isfinite() globalnie
#include "telemetria_wycinek.h"

static std::string json(const TelemetrySample &s) {
  char buf[1024]; size_t used = 0; bool first = true;
  telemetryAppendPathForSample(s, buf, sizeof buf, used, first);
  return std::string(buf, used);
}
static TelemetrySample probka() {
  TelemetrySample s; std::memset(&s, 0, sizeof s);
  s.sessionNonce = 0x01785F60363BD93EULL; s.sampleSeq = 4628; s.unixTs = 1795737656; s.monoMs = 277681300;
  s.tZewnX10 = 19; s.rhX10 = 834; s.pressureX10 = 10163; s.dym = 199; s.klapaDeg = 180; s.syberkoDeg = 90;
  s.qualityBits = 0x130000UL; s.simulatedBits = 0; for (int i = 0; i < 11; ++i) s.ageS[i] = 10 + i;
  s.tDhtDiagX10 = TEMP_DIAG_MISSING; s.tempDiagReserved = 0;
  return s;
}

int main() {
  std::puts("[1] stara probka (bez znacznika TEMP_DIAG / bez bitu DHT): JSON jak dotad, bez dht_*");
  { TelemetrySample s = probka();
    std::string j = json(s);
    SPRAWDZ(j.find("\"a\":[19,0,0,0,0,0,0,0,834,10163,199]") != std::string::npos, "tablica a");
    SPRAWDZ(j.find("\"sim\":0,") != std::string::npos, "sim=0");
    SPRAWDZ(j.find("dht_") == std::string::npos && j.find("t_zewn_2") == std::string::npos, "bez dht_kod/dht_surowa/t_zewn_2");
    SPRAWDZ(j.find("\"2026/11/27/01785F60363BD93E-00001214\":{") != std::string::npos, "sciezka rekordu jak w eksporcie 26.11");
  }
  std::puts("[2] probka z 3.58.22: ramka klona (kod 0, surowa -3275.3) -> dht_kod=0, dht_surowa=-3275.3, t_zewn_2=-1.5");
  { TelemetrySample s = probka();
    g_tempDiag = -1.5f; g_dhtBylOdczyt = true; g_dhtOstatniKod = 0; g_dhtOstatniaSurowaX10 = -32753;
    SPRAWDZ(telemetryWypelnijDht(s), "fragment telemetryBuildSnapshot()");
    SPRAWDZ(s.tempDiagReserved == -32753 && (s.simulatedBits & 0x8000U) && ((s.simulatedBits >> 11) & 0xF) == 0, "bity: obecne, kod 0, surowa x10 w reserved");
    std::string j = json(s);
    SPRAWDZ(j.find("\"t_zewn_2\":-1.5,") != std::string::npos, "t_zewn_2=-1.5");
    SPRAWDZ(j.find("\"dht_kod\":0,\"dht_surowa\":-3275.3,") != std::string::npos, "dht_kod=0, dht_surowa=-3275.3");
    SPRAWDZ(j.find("\"sim\":0,") != std::string::npos, "sim nadal 0 (bity DHT zamaskowane)");
    SPRAWDZ(j.find("\"q\":1245184,") != std::string::npos, "q bez znacznika TEMP_DIAG (jak dotad)");
  }
  std::puts("[3] blad biblioteki (kod 6 CHECKSUM): dht_kod=6, BEZ dht_surowa; symulowane kanaly w sim nietkniete");
  { TelemetrySample s = probka(); s.simulatedBits = 0x0555U;
    g_tempDiag = NAN; g_dhtOstatniKod = 6; g_dhtOstatniaSurowaX10 = 0;
    telemetryWypelnijDht(s);
    std::string j = json(s);
    SPRAWDZ(j.find("\"dht_kod\":6,") != std::string::npos && j.find("dht_surowa") == std::string::npos, "dht_kod=6 bez surowej");
    SPRAWDZ(j.find("\"sim\":1365,") != std::string::npos, "sim = 0x0555 = 1365 (maska kanalow), mimo bitow DHT w strukturze");
    SPRAWDZ(s.simulatedBits == (0x0555U | 0x8000U | (6U << 11)), "struktura: kanaly + obecne + kod 6");
    SPRAWDZ(j.find("t_zewn_2") == std::string::npos, "t_zewn_2 brak (NaN)");
  }
  std::puts("[4] granice surowej: +80.0, 0.0, -0.1 (znak-modul), +409.5 (klon 12-bit), -3276.7 (0xFFFF), kod 15");
  { struct P { int16_t x10; const char *ocz; } p[] = {
      {800, "\"dht_surowa\":80.0,"}, {0, "\"dht_surowa\":0.0,"}, {-1, "\"dht_surowa\":-0.1,"},
      {4095, "\"dht_surowa\":409.5,"}, {-32767, "\"dht_surowa\":-3276.7,"}, {32767, "\"dht_surowa\":3276.7,"} };
    for (auto &q : p) {
      TelemetrySample s = probka(); g_dhtOstatniKod = 0; g_dhtOstatniaSurowaX10 = q.x10; telemetryWypelnijDht(s);
      SPRAWDZ(json(s).find(q.ocz) != std::string::npos, q.ocz);
    }
    TelemetrySample s = probka(); g_dhtOstatniKod = 15; telemetryWypelnijDht(s);
    SPRAWDZ(json(s).find("\"dht_kod\":15,") != std::string::npos, "kod 15 (maks. 4 bity)");
    g_dhtOstatniKod = 0x1F; TelemetrySample s2 = probka(); telemetryWypelnijDht(s2);
    SPRAWDZ(((s2.simulatedBits >> 11) & 0xF) == 0xF && (s2.simulatedBits & 0x07FF) == 0, "kod > 15 obciety do 4 bitow, kanaly nietkniete");
  }
  std::puts("[5] przed pierwszym odczytem DHT po starcie (g_dhtBylOdczyt=false): bez pol, reserved=0");
  { TelemetrySample s = probka(); g_dhtBylOdczyt = false; g_dhtOstatniKod = 0; g_dhtOstatniaSurowaX10 = 123;
    telemetryWypelnijDht(s);
    SPRAWDZ(s.tempDiagReserved == 0 && !(s.simulatedBits & 0x8000U), "nic nie wpisane");
    SPRAWDZ(json(s).find("dht_") == std::string::npos, "JSON bez dht_*");
  }
  std::puts("[6] ABI: 80 B, offsety bez zmian");
  SPRAWDZ(sizeof(TelemetrySample) == 80 && offsetof(TelemetrySample, tempDiagReserved) == 78 && offsetof(TelemetrySample, simulatedBits) == 52, "80 B / 78 / 52");
  std::printf("\nTELEMETRIA-DHT: %d sprawdzen, %d bledow\n", sprawdzen, bledy);
  return bledy ? 1 : 0;
}

// Test na PC filtra Zew 2 (DHT22) z main_centrala.cpp v3.58.20 — blok
// [v3.58.9 ZEW2-FILTR] + [v3.58.10] ponowienie po bledzie + [v3.58.20] dekodowanie
// ujemnych z klona (U2), wyciety przez wytnij_zew2_filtr.py (zew2_filtr_wycinek.h).
// [1]-[13] = testy z paczki 3.58.10 bez zmian (regresja), [14]-[16] nowe.
// Kompilacja: g++ -std=c++17 -Wall -Wextra -Werror -I<katalog z wycinkiem> test_zew2_filtr.cpp
// Scenariusze odpowiadaja opisowi w naglowku pliku centrali ([2] a-d).
#include <math.h>   // isfinite/fabsf globalnie, jak w Arduino.h na ESP32
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static const uint8_t DHT_OK = 0;          // kod biblioteki dhtESP32-rmt
static const uint8_t DHT_TIMEOUT = 2;     // dowolny kod != DHT_OK
#include "zew2_filtr_wycinek.h"

static int bledy = 0, sprawdzen = 0;
#define SPRAWDZ(w, opis) do { ++sprawdzen; if (!(w)) { ++bledy; std::printf("  BLAD [%s:%d] %s\n", __FILE__, __LINE__, opis); } } while (0)
static bool rowne(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static Zew2Filtr nowy() { Zew2Filtr f = {NAN, NAN, 0, false, 0, 0, 0, 0, false, 0, 0, 0}; return f; }
// Jedna ramka: dhtErr, t, rh, czas -> decyzja; *wynik = to, co trafia do zapiszTempDiag()
static Zew2Decyzja ramka(Zew2Filtr &f, float t, unsigned long nowMs, float *wynik, float rh = 90.0f, uint8_t err = DHT_OK) {
  return zew2Filtruj(f, zew2RamkaPoprawna(err, t, rh), t, nowMs, wynik);
}

int main() {
  float w;

  std::puts("[1] pierwszy odczyt po starcie przyjety bez weryfikacji");
  { Zew2Filtr f = nowy();
    SPRAWDZ(ramka(f, 2.0f, 1000, &w) == ZEW2_PRZYJETY, "pierwszy odczyt -> PRZYJETY");
    SPRAWDZ(rowne(w, 2.0f) && rowne(f.ostatniaDobra, 2.0f) && f.przyjete == 1, "zapisany 2.0");
  }

  std::puts("[2] zwykly dryf (<= 3 C) przyjmowany od razu");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 0, &w);
    SPRAWDZ(ramka(f, 2.4f, 120000, &w) == ZEW2_PRZYJETY && rowne(w, 2.4f), "+0.4");
    SPRAWDZ(ramka(f, 5.4f, 240000, &w) == ZEW2_PRZYJETY && rowne(w, 5.4f), "+3.0 (rowno prog) jeszcze przechodzi");
    SPRAWDZ(ramka(f, 2.5f, 360000, &w) == ZEW2_PRZYJETY, "-2.9");
    SPRAWDZ(f.przyjete == 4 && f.odrzucone == 0 && !f.weryfikacja, "liczniki");
  }

  std::puts("[3] pik lustrzany (+2.1 -> -2.1, przeklamany bit znaku): powtorka pokazuje prawde");
  { Zew2Filtr f = nowy(); ramka(f, 2.1f, 0, &w);
    SPRAWDZ(ramka(f, -2.1f, 120000, &w) == ZEW2_DO_WERYFIKACJI, "-2.1 -> DO_WERYFIKACJI");
    SPRAWDZ(std::isnan(w) && f.weryfikacja && rowne(f.kandydat, -2.1f), "nic nie zapisane, kandydat -2.1");
    SPRAWDZ(rowne(f.ostatniaDobra, 2.1f), "ostatnia dobra bez zmian");
    SPRAWDZ(ramka(f, 2.1f, 123000, &w) == ZEW2_PRZYJETY, "powtorka 2.1 pasuje do historii -> PRZYJETY");
    SPRAWDZ(rowne(w, 2.1f) && !f.weryfikacja && std::isnan(f.kandydat), "zapisane 2.1, weryfikacja skonczona");
    SPRAWDZ(f.odrzucone == 1 && f.potwierdzone == 0 && f.przyjete == 2, "liczniki: 1 odrzucony");
  }

  std::puts("[4] prawdziwy skok > 3 C: dwa zgodne odczyty -> POTWIERDZONY po 3 s");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 0, &w);
    SPRAWDZ(ramka(f, 6.0f, 120000, &w) == ZEW2_DO_WERYFIKACJI, "6.0 -> DO_WERYFIKACJI");
    SPRAWDZ(ramka(f, 6.4f, 123000, &w) == ZEW2_POTWIERDZONY, "6.4 (roznica 0.4 <= 0.5) -> POTWIERDZONY");
    SPRAWDZ(rowne(w, 6.4f) && rowne(f.ostatniaDobra, 6.4f), "zapisana powtorka 6.4");
    SPRAWDZ(f.potwierdzone == 1 && f.odrzucone == 0, "liczniki");
  }

  std::puts("[5] dwa niezgodne odbiegajace odczyty -> ODRZUCONY, nastepny cykl normalnie");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 0, &w);
    SPRAWDZ(ramka(f, 9.0f, 120000, &w) == ZEW2_DO_WERYFIKACJI, "9.0 -> weryfikacja");
    SPRAWDZ(ramka(f, 15.0f, 123000, &w) == ZEW2_ODRZUCONY, "15.0 nie pasuje ani do 9.0 ani do 2.0 -> ODRZUCONY");
    SPRAWDZ(std::isnan(w) && rowne(f.ostatniaDobra, 2.0f) && !f.weryfikacja, "nic nie zapisane, ostatnia 2.0");
    SPRAWDZ(ramka(f, 2.2f, 240000, &w) == ZEW2_PRZYJETY && rowne(w, 2.2f), "2.2 po 120 s -> PRZYJETY");
    SPRAWDZ(f.odrzucone == 1, "jeden odrzucony (kandydat); powtorka nie liczona drugi raz");
  }

  std::puts("[6] kontrola calej ramki: RH i t poza zakresem, blad biblioteki");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 0, &w);
    SPRAWDZ(ramka(f, 2.0f, 120000, &w, 3366.8f) == ZEW2_ZLA_RAMKA && std::isnan(w), "RH 3366.8 % (bit 7 bajtu RH) -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, 2.0f, 240000, &w, 0.0f) == ZEW2_ZLA_RAMKA, "RH 0.0 % -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, 2.0f, 360000, &w, 0.9f) == ZEW2_ZLA_RAMKA, "RH 0.9 % -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, 2.0f, 480000, &w, 100.0f) == ZEW2_PRZYJETY, "RH 100.0 % -> OK (nasycenie)");
    SPRAWDZ(ramka(f, 2.0f, 600000, &w, 100.1f) == ZEW2_ZLA_RAMKA, "RH 100.1 % -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, -40.1f, 720000, &w) == ZEW2_ZLA_RAMKA, "t -40.1 -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, 80.1f, 840000, &w) == ZEW2_ZLA_RAMKA, "t 80.1 -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, NAN, 960000, &w) == ZEW2_ZLA_RAMKA, "t NaN -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, 2.0f, 1080000, &w, NAN) == ZEW2_ZLA_RAMKA, "RH NaN -> ZLA_RAMKA");
    SPRAWDZ(ramka(f, 2.0f, 1200000, &w, 90.0f, DHT_TIMEOUT) == ZEW2_ZLA_RAMKA, "kod != DHT_OK -> ZLA_RAMKA");
    SPRAWDZ(f.zleRamki == 9 && rowne(f.ostatniaDobra, 2.0f), "9 zlych ramek, ostatnia dobra bez zmian");
    SPRAWDZ(!zew2RamkaPoprawna(DHT_OK, 0.0f, 50.0f) == false, "0.0 C to prawidlowa temperatura");
  }

  std::puts("[7] zla ramka w trakcie weryfikacji kasuje kandydata");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 0, &w);
    ramka(f, -2.0f, 120000, &w);
    SPRAWDZ(f.weryfikacja, "weryfikacja trwa");
    SPRAWDZ(ramka(f, -2.0f, 123000, &w, 90.0f, DHT_TIMEOUT) == ZEW2_ZLA_RAMKA, "timeout DHT");
    SPRAWDZ(!f.weryfikacja && std::isnan(f.kandydat) && f.odrzucone == 1, "kandydat skasowany, policzony jako odrzucony");
    SPRAWDZ(ramka(f, 2.0f, 240000, &w) == ZEW2_PRZYJETY, "nastepny cykl normalnie");
  }

  std::puts("[8] powtorka, ktora nie doszla do skutku przez 15 s, przepada");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 0, &w);
    ramka(f, 7.0f, 120000, &w);
    SPRAWDZ(ramka(f, 7.1f, 120000 + 15001, &w) == ZEW2_DO_WERYFIKACJI, "po 15 s odczyt 7.1 to NOWY kandydat, nie potwierdzenie");
    SPRAWDZ(f.odrzucone == 1 && rowne(f.kandydat, 7.1f), "stary kandydat odrzucony, nowy 7.1");
    SPRAWDZ(ramka(f, 7.1f, 120000 + 18000, &w) == ZEW2_POTWIERDZONY, "i potwierdzony 3 s pozniej");
  }

  std::puts("[9] przeklamany PIERWSZY odczyt po starcie: filtr sam sie naprawia");
  { Zew2Filtr f = nowy();
    SPRAWDZ(ramka(f, -2.0f, 0, &w) == ZEW2_PRZYJETY, "start od -2.0 (smiec) - przyjety, bo brak historii");
    SPRAWDZ(ramka(f, 2.0f, 120000, &w) == ZEW2_DO_WERYFIKACJI, "prawdziwe 2.0 jest 'podejrzane'");
    SPRAWDZ(ramka(f, 2.1f, 123000, &w) == ZEW2_POTWIERDZONY && rowne(f.ostatniaDobra, 2.1f), "powtorka potwierdza 2.1 - historia naprawiona");
    SPRAWDZ(ramka(f, 2.0f, 240000, &w) == ZEW2_PRZYJETY, "dalej normalnie");
  }

  std::puts("[10] przejscia przez 0 C: male |t| bez powtorki, wieksze z powtorka");
  { Zew2Filtr f = nowy(); ramka(f, 0.3f, 0, &w);
    SPRAWDZ(ramka(f, -0.2f, 120000, &w) == ZEW2_PRZYJETY, "0.3 -> -0.2 bez powtorki");
    SPRAWDZ(ramka(f, -0.4f, 240000, &w) == ZEW2_PRZYJETY, "-0.2 -> -0.4");
    SPRAWDZ(ramka(f, 0.4f, 360000, &w) == ZEW2_PRZYJETY, "-0.4 -> 0.4 (|t| < 0.5) bez powtorki");
    SPRAWDZ(ramka(f, 0.6f, 480000, &w) == ZEW2_PRZYJETY, "0.4 -> 0.6");
    SPRAWDZ(ramka(f, -0.6f, 600000, &w) == ZEW2_DO_WERYFIKACJI, "0.6 -> -0.6 lustrzane -> powtorka");
    SPRAWDZ(ramka(f, -0.7f, 603000, &w) == ZEW2_POTWIERDZONY, "-0.7 potwierdza (prawdziwy mroz)");
  }

  std::puts("[11] przepelnienie millis() w trakcie weryfikacji");
  { Zew2Filtr f = nowy(); ramka(f, 2.0f, 4294960000UL, &w);
    SPRAWDZ(ramka(f, 8.0f, 4294966000UL, &w) == ZEW2_DO_WERYFIKACJI, "kandydat tuz przed przepelnieniem");
    SPRAWDZ(ramka(f, 8.1f, 1704UL, &w) == ZEW2_POTWIERDZONY, "powtorka 3 s pozniej (po przepelnieniu) nadal potwierdza");
  }

  std::puts("[12] dwa piki z rzedu w kolejnych cyklach: zaden nie trafia na wykres");
  { Zew2Filtr f = nowy(); ramka(f, 3.0f, 0, &w);
    SPRAWDZ(ramka(f, -3.0f, 120000, &w) == ZEW2_DO_WERYFIKACJI && std::isnan(w), "pik 1");
    SPRAWDZ(ramka(f, 3.0f, 123000, &w) == ZEW2_PRZYJETY, "powtorka 1 = prawda");
    SPRAWDZ(ramka(f, -3.1f, 240000, &w) == ZEW2_DO_WERYFIKACJI && std::isnan(w), "pik 2");
    SPRAWDZ(ramka(f, 2.9f, 243000, &w) == ZEW2_PRZYJETY && rowne(w, 2.9f), "powtorka 2 = prawda");
    SPRAWDZ(f.odrzucone == 2 && f.przyjete == 3, "liczniki");
  }

  std::puts("[13] [v3.58.10] ponowienie po bledzie: zla ramka / odrzucony -> za 5 s, max 2 w cyklu");
  { Zew2Filtr f = nowy(); ramka(f, 3.0f, 0, &w);
    Zew2Decyzja d = ramka(f, 3.1f, 120000, &w, 3366.8f);           // zla ramka (RH), odczyt cykliczny
    zew2PlanujPonowienie(f, d, true, 120000);
    SPRAWDZ(d == ZEW2_ZLA_RAMKA && f.ponowienie && f.ponowieniaWCyklu == 1 && f.ponowienia == 1, "po zlej ramce zaplanowane 1. ponowienie");
    SPRAWDZ(!zew2PonowienieGotowe(f, 123000) && zew2PonowienieGotowe(f, 125000), "gotowe dopiero po 5 s (3 s: nie, 5 s: tak)");
    d = ramka(f, NAN, 126000, &w, NAN, DHT_TIMEOUT);                  // ponowienie: timeout
    zew2PlanujPonowienie(f, d, false, 126000);
    SPRAWDZ(f.ponowienie && f.ponowieniaWCyklu == 2, "po timeout 2. ponowienie");
    d = ramka(f, NAN, 132000, &w, NAN, DHT_TIMEOUT);                  // 2. ponowienie: tez timeout
    zew2PlanujPonowienie(f, d, false, 132000);
    SPRAWDZ(!f.ponowienie && f.ponowieniaWCyklu == 2 && f.ponowienia == 2, "limit 2 w cyklu: trzeciego ponowienia nie ma");
    d = ramka(f, 3.2f, 240000, &w);                                   // nastepny cykl: dobry
    zew2PlanujPonowienie(f, d, true, 240000);
    SPRAWDZ(d == ZEW2_PRZYJETY && !f.ponowienie && f.ponowieniaWCyklu == 0, "nowy cykl zeruje limit, dobry odczyt = bez ponowienia");
    // odrzucony po powtorce -> ponowienie; podejrzany sam w sobie -> NIE (ma powtorke po 3 s)
    d = ramka(f, 9.0f, 360000, &w); zew2PlanujPonowienie(f, d, true, 360000);
    SPRAWDZ(d == ZEW2_DO_WERYFIKACJI && !f.ponowienie && f.weryfikacja, "podejrzany: powtorka po 3 s, bez ponowienia");
    d = ramka(f, 14.0f, 363000, &w); zew2PlanujPonowienie(f, d, false, 363000);
    SPRAWDZ(d == ZEW2_ODRZUCONY && f.ponowienie && f.ponowieniaWCyklu == 1 && !f.weryfikacja, "odrzucony: ponowienie za 5 s");
    d = ramka(f, 3.3f, 369000, &w); zew2PlanujPonowienie(f, d, false, 369000);
    SPRAWDZ(d == ZEW2_PRZYJETY && rowne(w, 3.3f) && !f.ponowienie, "ponowienie daje dobra wartosc -> przyjeta, koniec");
    // ponowienie przez przepelnienie millis()
    f.ponowienie = true; f.ponowienieOdMs = 4294966000UL;
    SPRAWDZ(!zew2PonowienieGotowe(f, 1000UL) && zew2PonowienieGotowe(f, 3704UL), "5 s liczone poprawnie przez granice 2^32");
  }


  // ── [v3.58.20] dekodowanie ujemnych z klona DHT22 (U2) ─────────────────────
  // Dokladnie to, co robi biblioteka dhtESP32-rmt z bajtami d2 d3 (znak-modul wg karty):
  //   temperature = ((d2 & 0x7f) * 256 + d3) * 0.1;  if (d2 & 0x80) temperature = -temperature;
  auto biblioteka = [](uint16_t raw) -> float {
    const unsigned d2 = raw >> 8, d3 = raw & 0xFF;
    float t = ((d2 & 0x7f) * 256 + d3) * 0.1;
    if (d2 & 0x80) t = -t;
    return t;
  };

  std::puts("[14] [v3.58.20] zew2DekodujU2: obraz U2 -0,1..-40,0 C przeliczany, reszta nietknieta");
  { float t;
    t = -3276.7f; SPRAWDZ(zew2DekodujU2(&t) && rowne(t, -0.1f), "-3276.7 -> -0.1 (raw 0xFFFF)");
    t = -3276.5f; SPRAWDZ(zew2DekodujU2(&t) && rowne(t, -0.3f), "-3276.5 -> -0.3");
    t = -3271.8f; SPRAWDZ(zew2DekodujU2(&t) && rowne(t, -5.0f), "-3271.8 -> -5.0");
    t = -3256.3f; SPRAWDZ(zew2DekodujU2(&t) && rowne(t, -20.5f), "-3256.3 -> -20.5");
    t = -3236.8f; SPRAWDZ(zew2DekodujU2(&t) && rowne(t, -40.0f), "-3236.8 -> -40.0 (granica zakresu DHT22)");
    t = -3236.7f; SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, -3236.7f), "-3236.7 (obraz -40.1, poza zakresem DHT22) zostaje -> zla ramka");
    t = -3276.8f; SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, -3276.8f), "-3276.8 (raw 0x8000 = -0.0 znak-modul, w U2 nieosiagalne) zostaje -> zla ramka");
    t = -3200.0f; SPRAWDZ(!zew2DekodujU2(&t), "-3200.0 zostaje -> zla ramka");
    t = 3276.7f;  SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, 3276.7f), "+3276.7 (dodatnie smieci) zostaje -> zla ramka");
    t = 0.0f;     SPRAWDZ(!zew2DekodujU2(&t) && t == 0.0f, "0.0 nietkniete");
    t = -0.0f;    SPRAWDZ(!zew2DekodujU2(&t) && t == 0.0f, "-0.0 nietkniete");
    t = -0.1f;    SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, -0.1f), "-0.1 (prawdziwy DHT22, znak-modul) nietkniete");
    t = -5.0f;    SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, -5.0f), "-5.0 nietkniete");
    t = -40.0f;   SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, -40.0f), "-40.0 nietkniete");
    t = 2.1f;     SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, 2.1f), "2.1 nietkniete");
    t = 80.0f;    SPRAWDZ(!zew2DekodujU2(&t) && rowne(t, 80.0f), "80.0 nietkniete");
    t = NAN;      SPRAWDZ(!zew2DekodujU2(&t) && std::isnan(t), "NaN zostaje NaN");
    t = -INFINITY; SPRAWDZ(!zew2DekodujU2(&t), "-inf zostaje");
  }

  std::puts("[15] [v3.58.20] wszystkie 400 wartosci -0,1..-40,0 C w obu kodowaniach przez biblioteke + dekoder");
  { int zleU2 = 0, zleZM = 0, zleDod = 0;
    for (int m = 1; m <= 400; ++m) {
      // klon: U2 (65536 - m) -> biblioteka daje -3276,8 + m/10 -> dekoder ma oddac -m/10
      float tu2 = biblioteka((uint16_t)(65536 - m));
      const bool bylo = zew2DekodujU2(&tu2);
      if (!bylo || !rowne(tu2, -m / 10.0f) || !zew2RamkaPoprawna(DHT_OK, tu2, 90.0f)) ++zleU2;
      // prawdziwy DHT22: znak-modul (0x8000 | m) -> biblioteka daje -m/10 -> dekoder NIE rusza
      float tzm = biblioteka((uint16_t)(0x8000 | m));
      if (zew2DekodujU2(&tzm) || !rowne(tzm, -m / 10.0f)) ++zleZM;
      // dodatnie m/10 (obie wersje tak samo) -> nietkniete
      float td = biblioteka((uint16_t)m);
      if (zew2DekodujU2(&td) || !rowne(td, m / 10.0f)) ++zleDod;
    }
    SPRAWDZ(zleU2 == 0, "U2: kazda z 400 wartosci odkodowana co do 0,1 C i przechodzi zew2RamkaPoprawna()");
    SPRAWDZ(zleZM == 0, "znak-modul: zadna z 400 wartosci nie jest ruszana");
    SPRAWDZ(zleDod == 0, "dodatnie: zadna z 400 wartosci nie jest ruszana");
    // U2 ponizej -40,0 (m = 401..1000, czyli do -100 C): poza zakresem czujnika -> dekoder nie rusza, ramka zla
    int przeszlo = 0;
    for (int m = 401; m <= 1000; ++m) { float t = biblioteka((uint16_t)(65536 - m)); if (zew2DekodujU2(&t) || zew2RamkaPoprawna(DHT_OK, t, 90.0f)) ++przeszlo; }
    SPRAWDZ(przeszlo == 0, "U2 ponizej -40 C: nic nie przechodzi (zla ramka jak dotad)");
  }

  std::puts("[16] [v3.58.20] scenariusz z 26.11: zejscie 0,3 -> 0,0 -> -0,1 -> -0,4 -> -1,2 na klonie U2 bez przerwy na wykresie");
  { Zew2Filtr f = nowy(); ramka(f, 0.3f, 0, &w);
    auto klon = [&](float prawdziwa) {           // co oddaje biblioteka dla klona U2 przy prawdziwej temperaturze
      const int m = (int)lroundf(prawdziwa * 10.0f);
      return biblioteka((uint16_t)(m < 0 ? 65536 + m : m));
    };
    float t;
    t = klon(0.0f);  zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 120000, &w) == ZEW2_PRZYJETY && rowne(w, 0.0f), "0.0 przyjete");
    t = klon(-0.1f); SPRAWDZ(rowne(t, -3276.7f), "klon: -0.1 przychodzi z biblioteki jako -3276.7 (tak odpadalo do 3.58.19)");
    SPRAWDZ(!zew2RamkaPoprawna(DHT_OK, t, 90.0f), "bez dekodera: zla ramka");
    SPRAWDZ(zew2DekodujU2(&t) && ramka(f, t, 240000, &w) == ZEW2_PRZYJETY && rowne(w, -0.1f), "z dekoderem: -0.1 przyjete od razu (|t| < 0,5: bez powtorki)");
    t = klon(-0.4f); zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 360000, &w) == ZEW2_PRZYJETY && rowne(w, -0.4f), "-0.4 przyjete");
    t = klon(-1.2f); zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 480000, &w) == ZEW2_PRZYJETY && rowne(w, -1.2f), "-1.2 przyjete");
    t = klon(-8.0f); zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 600000, &w) == ZEW2_DO_WERYFIKACJI, "-8.0 (skok 6,8) -> powtorka jak dla kazdej innej wartosci");
    t = klon(-7.9f); zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 603000, &w) == ZEW2_POTWIERDZONY && rowne(w, -7.9f), "powtorka -7.9 potwierdza");
    t = klon(1.5f);  zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 720000, &w) == ZEW2_DO_WERYFIKACJI, "powrot na plus 1.5 (skok 9,4) -> powtorka");
    t = klon(1.6f);  zew2DekodujU2(&t); SPRAWDZ(ramka(f, t, 723000, &w) == ZEW2_POTWIERDZONY && rowne(w, 1.6f), "powtorka 1.6 potwierdza - dodatnie bez zmian");
  }

  std::printf("\nZEW2-FILTR: %d sprawdzen, %d bledow\n", sprawdzen, bledy);
  return bledy ? 1 : 0;
}

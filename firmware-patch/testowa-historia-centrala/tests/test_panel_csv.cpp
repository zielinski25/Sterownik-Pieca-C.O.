// Test z oryginalnym kodem centrali wycietym z main_centrala.cpp przez
// tests/wytnij_blok.py z paczki v3.58.6 oraz jej hostowym harness testowym.
#include <fstream>
#include <iostream>
#define main oryginalny_main_testow
#include "test_panel_hist.cpp"
#undef main

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  std::vector<HistoriaProbka> k;
  std::vector<HistoriaProbkaDluga> d;
  std::string line;
  std::ifstream inK(std::string(argv[1]) + "/hist_short.csv");
  std::ifstream inD(std::string(argv[1]) + "/hist_long.csv");
  if (!inK || !inD) return 2;
  while (std::getline(inK, line)) {
    auto c = podziel(line, ','); if (c.size() != 14) return 2;
    HistoriaProbka p{};
    snprintf(p.etykieta, sizeof(p.etykieta), "%s", c[0].c_str());
    p.t_zewn=std::stof(c[1]); p.t_bojler=std::stof(c[2]); p.t_ogrz=std::stof(c[3]);
    p.t_ogrz_sr=std::stof(c[4]); p.t_powrot=std::stof(c[5]); p.t_panel=std::stof(c[6]); p.t_pokoj=std::stof(c[7]);
    p.pompa=c[8]=="1"; p.cisnienie=std::stof(c[9]); p.wilgotnosc=std::stof(c[10]);
    p.klapa_proc=std::stoi(c[11]); p.syberko_proc=std::stoi(c[12]); p.czas_unix=std::stoul(c[13]); k.push_back(p);
  }
  while (std::getline(inD, line)) {
    auto c = podziel(line, ','); if (c.size() != 36) return 2;
    HistoriaProbkaDluga p{};
    snprintf(p.etykieta, sizeof(p.etykieta), "%s", c[0].c_str());
    int j=1;
#define TRZY(name) p.name##_sr=std::stof(c[j++]); p.name##_min=std::stof(c[j++]); p.name##_max=std::stof(c[j++]);
    TRZY(t_zewn) TRZY(t_bojler) TRZY(t_ogrz) TRZY(t_ogrz_sr) TRZY(t_powrot) TRZY(t_panel) TRZY(t_pokoj) TRZY(cisnienie) TRZY(wilgotnosc)
#undef TRZY
#define TRZY_INT(name) p.name##_sr=std::stoi(c[j++]); p.name##_min=std::stoi(c[j++]); p.name##_max=std::stoi(c[j++]);
    TRZY_INT(klapa_proc) TRZY_INT(syberko_proc)
#undef TRZY_INT
    p.czas_unix=std::stoul(c[j++]); p.pompa=std::stoi(c[j]); d.push_back(p);
  }
  if (k.size()!=1440 || d.size()!=169) return 2;
  ustawKrotki(k); ustawDlugi(d); ustawZegar(k.back().czas_unix);
  for (const char* tryb : {"godz", "dzien", "tydz"}) {
    Odpowiedz r=wywolaj(tryb,96);
    if (r.kod!=200 || !r.poprawnaBudowa || r.wpisy.size()!=96 || ilePrzerw(r)>1) {
      std::cerr << "NIEPOWODZENIE: " << tryb << "\n"; return 1;
    }
    std::cout << "OK " << tryb << ": " << r.wpisy.size() << " punktow, " << ilePrzerw(r) << " przerw\n";
  }
  wyczyscBufory();
}

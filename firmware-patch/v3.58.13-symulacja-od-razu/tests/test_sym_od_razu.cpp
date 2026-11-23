// Test na PC (v3.58.13 SYM-OD-RAZU): wycina symulacjaUstawOdczytOdRazu() z
// main_centrala.cpp i sprawdza, ze kazde z 9 pol trafia do WLASCIWEJ zmiennej
// odczytu, a nieznane pole / "dym" nie ruszaja niczego. Kompilacja:
//   python3 wytnij_sym_od_razu.py ../src/main_centrala.cpp > /tmp/sym_od_razu.inc
//   g++ -std=c++17 -Wall -Wextra -Werror test_sym_od_razu.cpp -o /tmp/t && /tmp/t
#include <cstdio>
#include <cstring>
#include <string>
struct String {                       // atrapa Arduino String - tylko to, czego uzywa funkcja
  std::string s; String(const char* c) : s(c) {}
  bool operator==(const char* c) const { return s == c; }
};
float temperaturaOgrzewania = 1, temperaturaPaneluSlonecznego = 2, temperaturaBojlera = 3,
      temperaturaPomieszczenia = 4, temperaturaNaZewnatrz = 5, temperaturaOgrzewania_Powrot = 6,
      temperaturaOgrzewania_Trociny = 7, cisnienieOdczytane = 8, wilgotnoscOdczytana = 9;
#include "/tmp/sym_od_razu.inc"
int bledy = 0;
static void spr(bool ok, const char* co) { printf("  [%s] %s\n", ok ? " OK " : "BLAD", co); if (!ok) ++bledy; }
static float suma() { return temperaturaOgrzewania + temperaturaPaneluSlonecznego + temperaturaBojlera + temperaturaPomieszczenia + temperaturaNaZewnatrz + temperaturaOgrzewania_Powrot + temperaturaOgrzewania_Trociny + cisnienieOdczytane + wilgotnoscOdczytana; }
int main() {
  struct { const char* pole; float* zm; float w; } t[] = {
    {"ogrz", &temperaturaOgrzewania, 71.5f}, {"panel", &temperaturaPaneluSlonecznego, 88.0f},
    {"bojler", &temperaturaBojlera, 80.0f}, {"pokoj", &temperaturaPomieszczenia, 22.5f},
    {"zewn", &temperaturaNaZewnatrz, -12.0f}, {"ogrz_powrot", &temperaturaOgrzewania_Powrot, 40.0f},
    {"ogrz_trociny", &temperaturaOgrzewania_Trociny, 35.0f}, {"cisnienie", &cisnienieOdczytane, 1013.0f},
    {"wilgotnosc", &wilgotnoscOdczytana, 55.0f} };
  for (auto& x : t) {
    const float przed = suma(); const float stara = *x.zm;
    symulacjaUstawOdczytOdRazu(String(x.pole), x.w);
    char opis[96]; snprintf(opis, sizeof opis, "symuluj %s %.1f -> tylko ta zmienna (%.1f -> %.1f)", x.pole, x.w, stara, *x.zm);
    spr(*x.zm == x.w && suma() - przed == x.w - stara, opis);
  }
  { const float przed = suma(); symulacjaUstawOdczytOdRazu(String("dym"), 900); spr(suma() == przed, "\"dym\" nie rusza zadnej zmiennej odczytu (wlasna petla czujnika dymu)"); }
  { const float przed = suma(); symulacjaUstawOdczytOdRazu(String("nieznane"), 1); spr(suma() == przed, "nieznane pole: nic sie nie dzieje"); }
  printf("%s: %d sprawdzen, %d BLAD\n", bledy ? "NIEZALICZONY" : "ZALICZONY", 11, bledy);
  return bledy ? 1 : 0;
}

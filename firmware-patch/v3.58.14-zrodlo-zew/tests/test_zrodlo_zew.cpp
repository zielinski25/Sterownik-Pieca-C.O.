// Test C++ ZEWN-ZRODLO (3.58.14): linie wyciete z main_centrala.cpp
// (wytnij_zrodlo_zew.py -> /tmp/zrodlo_zew.inc) skompilowane g++ -Wall -Wextra
// -Werror z atrapami String/Preferences. 12 sprawdzen.
#include <cstdio>
#include <string>
#include <map>
typedef unsigned char byte;
struct String {   // minimalna atrapa Arduino String (bez dziedziczenia po std::string - inaczej operator+ jest dwuznaczny)
  std::string s;
  String() {}
  String(const char* c) : s(c) {}
  String(const std::string& c) : s(c) {}
  String(int v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned char v) : s(std::to_string((int)v)) {}
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  bool operator==(const char* o) const { return s == o; }
  size_t find(const char* o) const { return s.find(o); }
};
static String operator+(const String& a, const String& b) { return String(a.s + b.s); }
static String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
#define F(x) String(x)
struct Preferences {
  std::map<std::string, int> nvs; int zapisy = 0;
  void putUChar(const char* k, unsigned char v) { nvs[k] = v; zapisy++; }
  unsigned char getUChar(const char* k, unsigned char d) { return nvs.count(k) ? (unsigned char)nvs[k] : d; }
} config;
#include "/tmp/zrodlo_zew.inc"
int main() {
  int bledy = 0;
  auto spr = [&](bool ok, const char* co) { printf("  [%s] %s\n", ok ? " OK " : "BLAD", co); if (!ok) bledy++; };
  String err;
  spr(zewnZrodlo == 1, "domyslnie zewnZrodlo = 1 (DS18B20) - zachowanie jak przed 3.58.14");
  spr(ustawZrodlo("zewnZrodlo", 2, err) && zewnZrodlo == 2 && config.nvs["zewnZrodlo"] == 2, "ustaw zewnZrodlo 2 -> zmienna 2, NVS \"zewnZrodlo\"=2");
  spr(ustawZrodlo("zewnZrodlo", 1, err) && zewnZrodlo == 1 && config.nvs["zewnZrodlo"] == 1, "ustaw zewnZrodlo 1 -> zmienna 1, NVS 1");
  int zap = config.zapisy;
  spr(!ustawZrodlo("zewnZrodlo", 0, err) && zewnZrodlo == 1 && err.find("1 (DS18B20) lub 2 (DHT22)") != std::string::npos, "ustaw zewnZrodlo 0 -> odrzucone z czytelnym bledem, zmienna bez zmian");
  spr(!ustawZrodlo("zewnZrodlo", 3, err) && zewnZrodlo == 1, "ustaw zewnZrodlo 3 -> odrzucone");
  spr(!ustawZrodlo("zewnZrodlo", -1, err) && zewnZrodlo == 1, "ustaw zewnZrodlo -1 -> odrzucone");
  spr(config.zapisy == zap, "odrzucone wartosci nie dotykaja NVS");
  spr(!ustawZrodlo("inne", 2, err), "inne pole nie trafia do tej galezi");
  config.nvs.clear(); zewnZrodlo = 9; wczytajZrodlo();
  spr(zewnZrodlo == 1, "start bez klucza w NVS -> 1");
  config.nvs["zewnZrodlo"] = 2; wczytajZrodlo(); bool a = (zewnZrodlo == 2);
  config.nvs["zewnZrodlo"] = 7; wczytajZrodlo();
  spr(a && zewnZrodlo == 1, "start z NVS 2 -> 2; z uszkodzonym NVS 7 -> 1");
  zewnZrodlo = 2;
  spr(statusPelny() == "\"zewnZrodlo\":2," && statusLan() == "\"zewnZrodlo\":2," && ustawieniaGet() == ",\"zewnZrodlo\":2", "JSON: /piec/status+/api/lan/status, /api/status (LCD), /api/ustawienia-get niosa \"zewnZrodlo\":2");
  spr(std::string("zewnZrodlo").size() <= 15, "klucz NVS <= 15 znakow");
  printf("%s\n", bledy ? "NIEZALICZONY" : "ZALICZONY: 12 sprawdzen OK");
  return bledy ? 1 : 0;
}

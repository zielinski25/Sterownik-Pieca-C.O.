// Atrapa Preferences.h (NVS) - wartosci w pamieci, w g_atrapa.nvs.
#pragma once
#include "Arduino.h"

class Preferences {
 public:
  bool begin(const char* przestrzen, bool tylkoOdczyt = false) { AtrapaBezPomiaru bezPomiaru; (void)tylkoOdczyt; ns_ = przestrzen ? przestrzen : ""; return true; }
  void end() {}
  bool isKey(const char* k) { AtrapaBezPomiaru bezPomiaru; return g_atrapa.nvs[ns_].count(k) > 0 || g_atrapa.nvsTeksty[ns_].count(k) > 0; }
  bool remove(const char* k) { AtrapaBezPomiaru bezPomiaru; g_atrapa.nvs[ns_].erase(k); g_atrapa.nvsTeksty[ns_].erase(k); return true; }
  bool clear() { AtrapaBezPomiaru bezPomiaru; g_atrapa.nvs[ns_].clear(); g_atrapa.nvsTeksty[ns_].clear(); return true; }

#define ATRAPA_NVS_TYP(Nazwa, Typ)                                                        \
  size_t put##Nazwa(const char* k, Typ v) { AtrapaBezPomiaru bezPomiaru; g_atrapa.nvs[ns_][k] = (long long)v; return sizeof(Typ); } \
  Typ get##Nazwa(const char* k, Typ domyslna = 0) {                                       \
    AtrapaBezPomiaru bezPomiaru;                                                          \
    auto& m = g_atrapa.nvs[ns_];                                                          \
    auto it = m.find(k);                                                                  \
    return it == m.end() ? domyslna : (Typ)it->second;                                    \
  }
  ATRAPA_NVS_TYP(Char, int8_t)
  ATRAPA_NVS_TYP(UChar, uint8_t)
  ATRAPA_NVS_TYP(Short, int16_t)
  ATRAPA_NVS_TYP(UShort, uint16_t)
  ATRAPA_NVS_TYP(Int, int32_t)
  ATRAPA_NVS_TYP(UInt, uint32_t)
  ATRAPA_NVS_TYP(Long, int32_t)
  ATRAPA_NVS_TYP(ULong, uint32_t)
  ATRAPA_NVS_TYP(Bool, bool)
#undef ATRAPA_NVS_TYP

  size_t putString(const char* k, const String& v) { AtrapaBezPomiaru bezPomiaru; g_atrapa.nvsTeksty[ns_][k] = v.c_str(); return v.length(); }
  String getString(const char* k, const String& domyslna = String()) {
    std::string wartosc;
    bool jest = false;
    {
      AtrapaBezPomiaru bezPomiaru;
      auto& m = g_atrapa.nvsTeksty[ns_];
      auto it = m.find(k);
      if (it != m.end()) { jest = true; wartosc = it->second; }
    }
    return jest ? String(wartosc.c_str()) : domyslna;
  }

 private:
  std::string ns_;
};

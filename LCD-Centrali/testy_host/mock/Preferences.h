// Atrapa Preferences.h (NVS) - wartosci w pamieci, w g_atrapa.nvs.
#pragma once
#include "Arduino.h"

class Preferences {
 public:
  bool begin(const char* przestrzen, bool tylkoOdczyt = false) { (void)tylkoOdczyt; ns_ = przestrzen ? przestrzen : ""; return true; }
  void end() {}
  bool isKey(const char* k) { return g_atrapa.nvs[ns_].count(k) > 0 || g_atrapa.nvsTeksty[ns_].count(k) > 0; }
  bool remove(const char* k) { g_atrapa.nvs[ns_].erase(k); g_atrapa.nvsTeksty[ns_].erase(k); return true; }
  bool clear() { g_atrapa.nvs[ns_].clear(); g_atrapa.nvsTeksty[ns_].clear(); return true; }

#define ATRAPA_NVS_TYP(Nazwa, Typ)                                                        \
  size_t put##Nazwa(const char* k, Typ v) { g_atrapa.nvs[ns_][k] = (long long)v; return sizeof(Typ); } \
  Typ get##Nazwa(const char* k, Typ domyslna = 0) {                                       \
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

  size_t putString(const char* k, const String& v) { g_atrapa.nvsTeksty[ns_][k] = v.c_str(); return v.length(); }
  String getString(const char* k, const String& domyslna = String()) {
    auto& m = g_atrapa.nvsTeksty[ns_];
    auto it = m.find(k);
    return it == m.end() ? domyslna : String(it->second.c_str());
  }

 private:
  std::string ns_;
};

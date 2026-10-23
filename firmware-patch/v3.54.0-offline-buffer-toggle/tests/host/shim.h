// shim.h — minimalne atrapy Arduino/ESP32 dla testow hostowych v3.54.0.
//
// Po co: funkcje bufora offline (NVS, ARM/STOP, restore, ACK komend) nie
// dotykaja sprzetu — dotykaja tylko Preferences, String, littlefsReady i
// g_telemetryRing. Dzieki tym atrapom ten sam kod z main_centrala.cpp
// kompiluje sie i wykonuje na komputerze (g++), bez ESP32 i bez PlatformIO.
//
// KLUCZOWE: atrapy NIE powielaja logiki z firmware. PutBool/getBool/begin
// zachowuja sie jak Preferences ESP32 (putBool zwraca 1 bajt, begin(true)
// na nieistniejacym namespace zwraca false), zeby test wykryl bledy
// w kodzie firmware, a nie w atrapie.

#ifndef SPOOLRT_HOST_SHIM_H
#define SPOOLRT_HOST_SHIM_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <map>
#include <vector>
#include <type_traits>

// ───────────────────────────────────────── String (podzbior Arduino)
class String {
 public:
  std::string s;

  String() {}
  String(const char* p) : s(p ? p : "") {}
  String(const std::string& v) : s(v) {}

  template <typename T,
            typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  String(T v) {
    if (std::is_same<T, bool>::value) {
      s = v ? "1" : "0";
    } else if (std::is_same<T, char>::value) {
      s.assign(1, static_cast<char>(v));
    } else {
      s = std::to_string(v);
    }
  }

  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* p) { s += (p ? p : ""); return *this; }

  friend String operator+(const String& a, const String& b) {
    String r; r.s = a.s + b.s; return r;
  }
  friend String operator+(const String& a, const char* b) {
    String r; r.s = a.s + (b ? b : ""); return r;
  }
  friend String operator+(const char* a, const String& b) {
    String r; r.s = std::string(a ? a : "") + b.s; return r;
  }

  bool operator==(const char* p) const { return s == (p ? p : ""); }
  bool operator==(const String& o) const { return s == o.s; }
  bool operator!=(const String& o) const { return s != o.s; }

  size_t length() const { return s.size(); }
  const char* c_str() const { return s.c_str(); }
};

// F() na ESP32 trzyma literał w flash; dla testu to zwykły const char*.
#define F(x) (x)

// ───────────────────────────────────────── NVS (Preferences)
struct NvsStore {
  std::map<std::string, std::map<std::string, uint8_t> > ns;
};

extern NvsStore g_nvs;
extern bool g_nvsWriteFails;  // symulacja: NVS odmawia zapisu
extern bool g_nvsOpenFails;   // symulacja: partycja NVS niedostepna

class Preferences {
 public:
  bool begin(const char* name, bool readonly = false) {
    if (g_nvsOpenFails) return false;
    ns_ = (name ? name : "");
    // ESP32: begin(readonly=true) na nieistniejacym namespace zwraca false.
    if (readonly && g_nvs.ns.find(ns_) == g_nvs.ns.end()) return false;
    open_ = true;
    return true;
  }

  size_t putBool(const char* key, bool v) {
    if (!open_ || g_nvsWriteFails) return 0;
    g_nvs.ns[ns_][key ? key : ""] = v ? 1 : 0;
    return sizeof(uint8_t);
  }

  bool getBool(const char* key, bool def = false) const {
    std::map<std::string, std::map<std::string, uint8_t> >::const_iterator it =
        g_nvs.ns.find(ns_);
    if (it == g_nvs.ns.end()) return def;
    std::map<std::string, uint8_t>::const_iterator k = it->second.find(key ? key : "");
    if (k == it->second.end()) return def;
    return k->second != 0;
  }

  void end() { open_ = false; }

 private:
  std::string ns_;
  bool open_ = false;
};

// ───────────────────────────────────────── logi (DLOG / zapiszLog)
enum { DC_SPOOL = 0, DC_SYSTEM = 1, DC_NET = 2 };
enum { DL_INFO = 0, DL_WARN = 1, DL_ERROR = 2 };

extern std::vector<std::string> g_logs;

#define DLOG(cat, lvl, msg) \
  do { (void)(cat); (void)(lvl); g_logs.push_back(std::string(msg)); } while (0)

void zapiszLog(const String& s);

// ───────────────────────────────────────── stan globalny firmware'u
extern bool littlefsReady;

struct TelemetryRingStub {
  bool ok = true;
  bool ready() const { return ok; }
};
extern TelemetryRingStub g_telemetryRing;

extern bool g_fbWorkerBusy;
bool fbLoopWorkerZajety();

extern bool g_spoolRuntimeArmed;
extern uint32_t g_spoolRuntimeOfflineSinceMs;
extern uint32_t g_spoolRuntimePagesCreated;
extern uint32_t g_spoolRuntimeSamplesSpilled;
extern uint32_t g_spoolRuntimeSamplesReplayed;

extern uint32_t g_nowMs;
uint32_t millis();

#endif  // SPOOLRT_HOST_SHIM_H

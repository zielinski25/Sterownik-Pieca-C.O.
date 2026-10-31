#pragma once
// 211: minimalny model Arduino/FreeRTOS na hosta. Modelowany jest WYLACZNIE
// swiat zewnetrzny (String, czas, semafory, alokacja, Serial, WiFi), zeby dac
// sie skompilowac PRAWDZIWY kod obu funkcji wysylajacych z centrali.
// Nie jest to Arduino i nie udaje Arduino - to atrapa do testu hostowego.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <chrono>

typedef bool boolean;
typedef uint8_t byte;

#define F(x) (x)
#define WL_CONNECTED 3

// ---------- String -------------------------------------------------------
class String {
 public:
  String() {}
  String(const char *p) : s_(p ? p : "") {}
  String(const std::string &s) : s_(s) {}
  explicit String(unsigned v) : s_(std::to_string(v)) {}
  explicit String(int v) : s_(std::to_string(v)) {}
  String(char c) : s_(1, c) {}
  size_t length() const { return s_.size(); }
  const char *c_str() const { return s_.c_str(); }
  bool reserve(size_t n) { s_.reserve(n); return true; }
  int indexOf(const char *p) const {
    const size_t k = s_.find(p);
    return k == std::string::npos ? -1 : static_cast<int>(k);
  }
  String &operator+=(char c) { s_ += c; return *this; }
  char operator[](size_t i) const { return s_[i]; }
  bool operator==(const char *p) const { return s_ == (p ? p : ""); }
  const std::string &std_str() const { return s_; }

  friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }
  friend String operator+(const String &a, const char *b) { return String(a.s_ + (b ? b : "")); }
  friend String operator+(const char *a, const String &b) { return String((a ? a : "") + b.s_); }

 private:
  std::string s_;
};

// ---------- czas (zegar wirtualny) --------------------------------------
extern unsigned long g_zegar211;
inline unsigned long millis() { return g_zegar211; }
inline void delay(unsigned long ms) { g_zegar211 += ms ? ms : 1UL; }

// ---------- alokacja z mozliwoscia awarii -------------------------------
extern int g_psmallocFail211;  // >0 -> ps_malloc zwraca nullptr
extern int g_mallocFail211;    // >0 -> model_malloc zwraca nullptr
void *model_ps_malloc(size_t n);
void *model_malloc(size_t n);
inline void *ps_malloc(size_t n) { return model_ps_malloc(n); }

// ---------- Serial ------------------------------------------------------
class SerialClass {
 public:
  void printf(const char *fmt, ...);
  void println(const char *s);
};
extern SerialClass Serial;

// ---------- FreeRTOS (tylko to, czego uzywaja funkcje) ------------------
typedef void *SemaphoreHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)
SemaphoreHandle_t xSemaphoreCreateMutex();
int xSemaphoreTake(SemaphoreHandle_t h, unsigned ticks);
int xSemaphoreGive(SemaphoreHandle_t h);
// Testy ustawiaja na true, zeby sprawdzic sciezke "blokada zajeta".
extern bool g_blokadaZajeta211;

// ---------- WiFi --------------------------------------------------------
class WiFiClass {
 public:
  int status() const { return stan_; }
  void ustaw(int s) { stan_ = s; }
 private:
  int stan_ = WL_CONNECTED;
};
extern WiFiClass WiFi;

// ---------- logi --------------------------------------------------------
enum DiagCat { DC_TELEGRAM = 0, DC_HISTORIA = 1 };
enum DiagLevel { DL_ERR = 0, DL_WARN = 1, DL_INFO = 2 };
void diagEmit(int cat, int level, const char *msg);
#define DLOG(cat, level, msg) diagEmit((cat), (level), (msg))

extern int g_diagErr211;   // ile razy padl DL_ERR
extern int g_diagWarn211;  // ile razy padl DL_WARN

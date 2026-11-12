// ============================================================================
// Atrapa Arduino.h (rdzen arduino-esp32 3.x) do budowania panelu na PC.
// Patrz atrapa.h - co jest modelowane, a co tylko "zeby sie skompilowalo".
// ============================================================================
#pragma once

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifdef __cplusplus
#include <algorithm>
#include <cmath>
#include <string>

#include "atrapa.h"

// Tak samo jak w cores/esp32/Arduino.h - min()/max() to std::min/std::max,
// czyli oba argumenty MUSZA miec ten sam typ (blad typow wyjdzie tu tak samo
// jak przy prawdziwym budowaniu).
using std::abs;
using std::isinf;
using std::isnan;
using std::max;
using std::min;
using ::round;

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x09
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define F(x) (x)
#define PSTR(x) (x)
#define RTC_DATA_ATTR
#define RTC_NOINIT_ATTR   // na PC zwykla zmienna globalna; test moze ja wypelnic przed setup() ("stan po restarcie")
#define IRAM_ATTR
#define portNUM_PROCESSORS 2
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define sq(x) ((x) * (x))

typedef enum { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db, ADC_12db = ADC_11db } adc_attenuation_t;

// ---------------------------------------------------------------------------
// String - na std::string; te metody, ktorych uzywa firmware i ArduinoJson.
// ---------------------------------------------------------------------------
class String {
 public:
  String() {}
  String(const char* c) : s_(c ? c : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(unsigned char v, unsigned char baza = 10) : s_(liczba((unsigned long)v, baza)) {}
  String(int v, unsigned char baza = 10) : s_(liczba((long)v, baza)) {}
  String(unsigned int v, unsigned char baza = 10) : s_(liczba((unsigned long)v, baza)) {}
  String(long v, unsigned char baza = 10) : s_(liczba(v, baza)) {}
  String(unsigned long v, unsigned char baza = 10) : s_(liczba(v, baza)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}
  String(float v, unsigned int miejsc = 2) : s_(ulamek(v, miejsc)) {}
  String(double v, unsigned int miejsc = 2) : s_(ulamek(v, miejsc)) {}

  const char* c_str() const { return s_.c_str(); }
  unsigned int length() const { return (unsigned int)s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  bool reserve(unsigned int n) { s_.reserve(n); return true; }
  char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
  char operator[](unsigned int i) const { return charAt(i); }
  char& operator[](unsigned int i) { return s_[i]; }

  bool concat(const String& o) { s_ += o.s_; return true; }
  bool concat(const char* c) { if (c) s_ += c; return true; }
  bool concat(const char* c, unsigned int n) { if (c) s_.append(c, n); return true; }
  bool concat(char c) { s_ += c; return true; }
  template <typename T> String& operator+=(const T& v) { s_ += String(v).s_; return *this; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* c) { if (c) s_ += c; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }

  bool equals(const String& o) const { return s_ == o.s_; }
  bool equals(const char* c) const { return s_ == (c ? c : ""); }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* c) const { return equals(c); }
  bool operator!=(const String& o) const { return s_ != o.s_; }
  bool operator!=(const char* c) const { return !equals(c); }
  bool operator<(const String& o) const { return s_ < o.s_; }
  bool startsWith(const String& p) const { return s_.rfind(p.s_, 0) == 0; }
  bool endsWith(const String& p) const {
    return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
  }

  int indexOf(char c, unsigned int od = 0) const { return pozycja(s_.find(c, od)); }
  int indexOf(const String& t, unsigned int od = 0) const { return pozycja(s_.find(t.s_, od)); }
  int lastIndexOf(char c) const { return pozycja(s_.rfind(c)); }
  String substring(unsigned int od) const { return od >= s_.size() ? String() : String(s_.substr(od)); }
  String substring(unsigned int od, unsigned int doo) const {
    if (od > doo) std::swap(od, doo);
    if (od >= s_.size()) return String();
    return String(s_.substr(od, doo - od));
  }
  long toInt() const { return atol(s_.c_str()); }
  float toFloat() const { return (float)atof(s_.c_str()); }
  void trim() {
    size_t a = s_.find_first_not_of(" \t\r\n");
    size_t b = s_.find_last_not_of(" \t\r\n");
    s_ = (a == std::string::npos) ? std::string() : s_.substr(a, b - a + 1);
  }
  void replace(const String& co, const String& naCo) {
    if (co.s_.empty()) return;
    for (size_t p = 0; (p = s_.find(co.s_, p)) != std::string::npos; p += naCo.s_.size()) s_.replace(p, co.s_.size(), naCo.s_);
  }
  void toUpperCase() { for (auto& c : s_) c = (char)toupper((unsigned char)c); }
  void toLowerCase() { for (auto& c : s_) c = (char)tolower((unsigned char)c); }

 private:
  std::string s_;
  static int pozycja(size_t p) { return p == std::string::npos ? -1 : (int)p; }
  static std::string liczba(long v, unsigned char baza) {
    char b[72];
    if (baza == 16) snprintf(b, sizeof(b), "%lx", (unsigned long)v);
    else snprintf(b, sizeof(b), "%ld", v);
    return b;
  }
  static std::string liczba(unsigned long v, unsigned char baza) {
    char b[72];
    snprintf(b, sizeof(b), baza == 16 ? "%lx" : "%lu", v);
    return b;
  }
  static std::string ulamek(double v, unsigned int miejsc) {
    char b[72];
    snprintf(b, sizeof(b), "%.*f", (int)miejsc, v);   // dla NaN daje "nan" - tak samo jak dtostrf() na ESP32
    return b;
  }
};

inline String operator+(const String& a, const String& b) { String w(a); w += b; return w; }
inline String operator+(const String& a, const char* b) { String w(a); w += b; return w; }
inline String operator+(const char* a, const String& b) { String w(a); w += b; return w; }
inline String operator+(const String& a, char b) { String w(a); w += b; return w; }
inline String operator+(const String& a, int b) { String w(a); w += String(b); return w; }
inline String operator+(const String& a, unsigned int b) { String w(a); w += String(b); return w; }
inline String operator+(const String& a, long b) { String w(a); w += String(b); return w; }
inline String operator+(const String& a, unsigned long b) { String w(a); w += String(b); return w; }
inline String operator+(const String& a, float b) { String w(a); w += String(b); return w; }
inline String operator+(const String& a, double b) { String w(a); w += String(b); return w; }

// ---------------------------------------------------------------------------
// Serial - zapis do g_atrapa.serial (test czyta stamtad log firmware'u).
// ---------------------------------------------------------------------------
class AtrapaSerial {
 public:
  void begin(unsigned long) {}
  void flush() {}
  int available() { return 0; }
  int read() { return -1; }
  void print(const char* s) { dopisz(s ? s : ""); }
  void print(const String& s) { dopisz(s.c_str()); }
  void print(char c) { char b[2] = {c, 0}; dopisz(b); }
  void print(int v) { dopisz(String(v).c_str()); }
  void print(unsigned int v) { dopisz(String(v).c_str()); }
  void print(long v) { dopisz(String(v).c_str()); }
  void print(unsigned long v) { dopisz(String(v).c_str()); }
  void print(double v, int miejsc = 2) { dopisz(String(v, (unsigned int)miejsc).c_str()); }
  void println() { dopisz("\n"); }
  template <typename T> void println(const T& v) { print(v); dopisz("\n"); }
  void println(double v, int miejsc) { print(v, miejsc); dopisz("\n"); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

 private:
  void dopisz(const char* s);
};
extern AtrapaSerial Serial;

// ---------------------------------------------------------------------------
// ESP, czas, GPIO, ADC, PWM
// ---------------------------------------------------------------------------
class AtrapaEsp {
 public:
  uint32_t getFreeHeap() { return 180000; }
  uint32_t getMinFreeHeap() { return 150000; }
  uint32_t getMaxAllocHeap() { return 110000; }
  // Na plytce ESP.restart() nie wraca. Tu: wyjatek AtrapaRestart, zeby kod
  // firmware'u za wywolaniem tez sie nie wykonal; lapie go test (biegnij()).
  [[noreturn]] void restart() { g_atrapa.restarty++; throw AtrapaRestart(); }
};
extern AtrapaEsp ESP;

inline unsigned long millis() { return g_atrapa.terazMs; }
inline unsigned long micros() { return g_atrapa.terazMs * 1000UL; }
inline void delay(unsigned long ms) { atrapa_uplyw_czasu(ms); }
inline void delayMicroseconds(unsigned int) {}
inline void yield() {}

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t v) { if (pin < 64) g_atrapa.gpio[pin] = v; }
inline int digitalRead(uint8_t pin) { return pin < 64 ? g_atrapa.gpio[pin] : 0; }
inline uint32_t analogReadMilliVolts(uint8_t pin) { return pin < 64 ? (uint32_t)g_atrapa.adcMv[pin] : 0; }
inline uint16_t analogRead(uint8_t pin) { return pin < 64 ? (uint16_t)(g_atrapa.adcMv[pin] * 4095 / 3300) : 0; }
inline void analogSetAttenuation(adc_attenuation_t) {}
inline void analogSetPinAttenuation(uint8_t, adc_attenuation_t) {}
inline bool ledcAttach(uint8_t, uint32_t, uint8_t) { return true; }
inline bool ledcDetach(uint8_t) { return true; }
inline bool ledcWrite(uint8_t, uint32_t) { return true; }
// Brzeczyk: ostatnia czestotliwosc i liczba jej zmian (0 Hz = cisza).
inline uint32_t ledcWriteTone(uint8_t, uint32_t f) {
  if (f != g_atrapa.tonHz) g_atrapa.zmianTonu++;
  g_atrapa.tonHz = f;
  return f;
}
inline long map(long x, long a, long b, long c, long d) { return (b == a) ? c : (x - a) * (d - c) / (b - a) + c; }
inline long random(long m) { return m > 0 ? rand() % m : 0; }
inline long random(long a, long b) { return b > a ? a + rand() % (b - a) : a; }

// ---------------------------------------------------------------------------
// IPAddress
// ---------------------------------------------------------------------------
class IPAddress {
 public:
  IPAddress() : b_{0, 0, 0, 0} {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : b_{a, b, c, d} {}
  uint8_t operator[](int i) const { return b_[i & 3]; }
  String toString() const {
    char t[20];
    snprintf(t, sizeof(t), "%u.%u.%u.%u", b_[0], b_[1], b_[2], b_[3]);
    return String(t);
  }

 private:
  uint8_t b_[4];
};

#endif  // __cplusplus

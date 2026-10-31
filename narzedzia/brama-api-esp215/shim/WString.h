#pragma once
// Shim "prawdziwego Arduino" dla testu hostowego. Odwzorowuje TYLKO to,
// czego wymaga ESPAsyncWebServer.h i naglowki firmware (etapy 211-214).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

class __FlashStringHelper;
#define PGM_P const char *
#define PROGMEM
#define ICACHE_FLASH_ATTR
#define F(x) (x)
#define FPSTR(x) (x)
typedef bool boolean;
typedef uint8_t byte;

class String {
 public:
  String() {}
  String(const char *p) : s_(p ? p : "") {}
  String(const std::string &s) : s_(s) {}
  explicit String(char c) : s_(1, c) {}
  explicit String(unsigned char v, unsigned char base = 10) { (void)base; s_ = std::to_string((int)v); }
  explicit String(int v) { s_ = std::to_string(v); }
  explicit String(unsigned int v) { s_ = std::to_string(v); }
  explicit String(long v) { s_ = std::to_string(v); }
  explicit String(unsigned long v) { s_ = std::to_string(v); }
  explicit String(float v, unsigned char dec = 2) { (void)dec; s_ = std::to_string(v); }
  explicit String(double v, unsigned char dec = 2) { (void)dec; s_ = std::to_string(v); }
  explicit String(long long v) { s_ = std::to_string(v); }
  explicit String(unsigned long long v) { s_ = std::to_string(v); }

  size_t length() const { return s_.size(); }
  const char *c_str() const { return s_.c_str(); }
  bool reserve(size_t n) { s_.reserve(n); return true; }
  bool concat(const char *p) { s_ += (p ? p : ""); return true; }
  bool concat(char c) { s_ += c; return true; }
  int indexOf(const char *p, size_t od = 0) const {
    const size_t k = s_.find(p ? p : "", od);
    return k == std::string::npos ? -1 : (int)k;
  }
  int indexOf(char c, size_t od = 0) const {
    const size_t k = s_.find(c, od);
    return k == std::string::npos ? -1 : (int)k;
  }
  String substring(size_t od) const { return String(od <= s_.size() ? s_.substr(od) : std::string()); }
  String substring(size_t od, size_t ile) const {
    if (od > s_.size()) return String();
    return String(s_.substr(od, ile));
  }
  long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }
  bool startsWith(const char *p) const {
    const std::string q(p ? p : "");
    return s_.compare(0, q.size(), q) == 0;
  }
  bool endsWith(const char *p) const {
    const std::string q(p ? p : "");
    return s_.size() >= q.size() && s_.compare(s_.size() - q.size(), q.size(), q) == 0;
  }
  bool equalsIgnoreCase(const char *p) const {
    const char *a = s_.c_str();
    const char *b = p ? p : "";
    if (std::strlen(a) != std::strlen(b)) return false;
    for (; *a; ++a, ++b) {
      char x = *a, y = *b;
      if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
      if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
      if (x != y) return false;
    }
    return true;
  }
  void clear() { s_.clear(); }
  String &operator+=(char c) { s_ += c; return *this; }
  String &operator+=(const char *p) { s_ += (p ? p : ""); return *this; }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  char operator[](size_t i) const { return s_[i]; }
  char &operator[](size_t i) { return s_[i]; }
  bool operator==(const char *p) const { return s_ == (p ? p : ""); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const char *p) const { return !(*this == p); }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator<(const String &o) const { return s_ < o.s_; }
  const std::string &std_str() const { return s_; }

  friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }
  friend String operator+(const String &a, const char *b) { return String(a.s_ + (b ? b : "")); }
  friend String operator+(const char *a, const String &b) { return String((a ? a : "") + b.s_); }
  friend String operator+(const String &a, char b) { return String(a.s_ + b); }

 private:
  std::string s_;
};

extern const String emptyString;

// Arduino.h — MINIMALNY STUB na PC, WYLACZNIE do sprawdzenia skladni main.cpp.
// Nie sluzy do uruchamiania firmware'u, tylko do "czy sie kompiluje" na g++.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#define F(x) (x)
#define PROGMEM

class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }

 private:
  std::string s_;
};

class SerialStub {
 public:
  void begin(unsigned long) {}
  template <typename T> void print(const T&) {}
  template <typename T> void println(const T&) {}
  void println() {}
  void printf(const char* fmt, ...) { (void)fmt; }
  int available() { return 0; }
  int read() { return -1; }
};
extern SerialStub Serial;

inline unsigned long millis() { static unsigned long m = 0; return m += 1000; }
inline void delay(unsigned long) {}
inline void disableLoopWDT() {}

class EspStub {
 public:
  uint32_t getFreeHeap() { return 200000; }
  uint32_t getMinFreeHeap() { return 150000; }
  void restart() {}
  const char* getSdkVersion() { return "stub"; }
  const char* getChipModel() { return "ESP32-C3"; }
  uint8_t getChipCores() { return 1; }
  uint32_t getCpuFreqMHz() { return 160; }
};
extern EspStub ESP;

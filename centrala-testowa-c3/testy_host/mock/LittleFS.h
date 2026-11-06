// LittleFS.h — stub na PC (tylko skladnia). Zobacz Arduino.h w tym katalogu.
// Odwzorowuje dokladnie te czesci API, ktorych uzywa src/main.cpp:
// LittleFS.begin(), LittleFS.exists(), LittleFS.open() oraz File::available(),
// File::readBytesUntil(), File::close().
#pragma once
#include <cstddef>

class File {
 public:
  int available() { return 0; }
  size_t readBytesUntil(char, char*, size_t) { return 0; }
  void close() {}
  operator bool() const { return false; }
};

class LittleFSStub {
 public:
  bool begin(bool = false) { return true; }
  bool exists(const char*) { return false; }
  File open(const char*, const char* = "r") { return File(); }
};

extern LittleFSStub LittleFS;

// Preferences.h — stub na PC (tylko skladnia).
#pragma once
#include "Arduino.h"

class Preferences {
 public:
  bool begin(const char*, bool = false) { return true; }
  bool isKey(const char*) { return false; }
  int32_t getInt(const char*, int32_t domyslna = 0) { return domyslna; }
  uint32_t getUInt(const char*, uint32_t domyslna = 0) { return domyslna; }
  size_t putInt(const char*, int32_t) { return 4; }
  size_t putUInt(const char*, uint32_t) { return 4; }
  size_t putBool(const char*, bool) { return 1; }
};

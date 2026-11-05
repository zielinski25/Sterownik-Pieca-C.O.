// WiFi.h — stub na PC (tylko skladnia). Zobacz Arduino.h w tym katalogu.
#pragma once
#include "Arduino.h"

class IPAddress {
 public:
  IPAddress() {}
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
  String toString() const { return String("192.168.4.1"); }
};

enum WiFiMode { WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA };

class WiFiClient {
 public:
  int available() { return 0; }
  int read() { return -1; }
  bool connected() { return false; }
  void print(const char*) {}
  void print(int) {}
  void print(unsigned) {}
  size_t write(const uint8_t*, size_t n) { return n; }
  void flush() {}
  void stop() {}
  operator bool() const { return false; }
};

class WiFiServer {
 public:
  explicit WiFiServer(uint16_t) {}
  void begin() {}
  void setNoDelay(bool) {}
  WiFiClient available() { return WiFiClient(); }
};

class WiFiStub {
 public:
  bool persistent(bool) { return true; }
  bool setSleep(bool) { return true; }
  bool mode(WiFiMode) { return true; }
  bool softAPConfig(IPAddress, IPAddress, IPAddress) { return true; }
  bool softAP(const char*, const char* = nullptr, int = 1, int = 0, int = 4) { return true; }
  IPAddress softAPIP() { return IPAddress(); }
  int softAPgetStationNum() { return 0; }
};
extern WiFiStub WiFi;

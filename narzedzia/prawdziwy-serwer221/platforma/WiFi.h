// [ETAP 221] WiFi na komputerze - CZESC NASZA.
// Biblioteka siega do WiFi tylko wtedy, gdy ASYNCWEBSERVER_WIFI_SUPPORTED == 1,
// a to zalezy od makr rdzenia ESP32. Tutaj ich nie ma, wiec jedyne, co musi
// istniec, to sam plik naglowka (ESPAsyncWebServer.h go includuje).
#pragma once

#include <Arduino.h>

class WiFiClass {
 public:
  IPAddress localIP() { return IPAddress(192, 168, 1, 100); }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  int status() { return 3; }
  void begin(const char *ssid = nullptr, const char *pass = nullptr) {
    (void)ssid;
    (void)pass;
  }
  void mode(int m) { (void)m; }
};
extern WiFiClass WiFi;

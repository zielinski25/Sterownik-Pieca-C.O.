// Atrapa HTTPClient.h - ekran Wykresy (GET /api/history). W testach zawsze
// "brak polaczenia" (kod ujemny), bo ta sciezka nie jest przedmiotem testow.
#pragma once
#include "Arduino.h"
#include "WiFi.h"

#define HTTP_CODE_OK 200

class HTTPClient {
 public:
  void setTimeout(uint32_t) {}
  void setConnectTimeout(int32_t) {}
  void useHTTP10(bool) {}
  void setReuse(bool) {}
  bool begin(const String&) { return true; }
  bool begin(WiFiClient&, const String&) { return true; }
  void addHeader(const String&, const String&) {}
  int GET() { return -1; }
  int POST(const String&) { return -1; }
  int getSize() { return 0; }
  String getString() { return String(); }
  WiFiClient* getStreamPtr() { return &klient_; }
  WiFiClient& getStream() { return klient_; }
  bool connected() { return false; }
  void end() {}
  static String errorToString(int) { return String("atrapa: brak polaczenia"); }

 private:
  WiFiClient klient_;
};

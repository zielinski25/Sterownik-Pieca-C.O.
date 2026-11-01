#pragma once
// [etap 215] Shim "Arduino na komputerze" dla bramy API.
//
// Odwzorowuje WYLACZNIE te nazwy, ktorych wymaga PRAWDZIWY
// ESPAsyncWebServer.h oraz naglowki firmware z etapow 211-214.
// Nie jest biblioteka i nie udaje pelnego API Arduino - kazda dodana
// nazwa ma byc potrzebna, inaczej shim przestaje chronic przed bledami.
//
// Czego CELOWO nie ma: operator bool w String (prawdziwe WString go nie ma,
// a jego brak wylapuje niejednoznacznosci w przeciazeniach), zadnych klas
// WiFi, OTA, HTTPClient - te naglowki nie sa tu kompilowane.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "WString.h"

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *, size_t) = 0;
  size_t print(const char *) { return 0; }
  size_t println(const char *) { return 0; }
  size_t printf(const char *, ...) { return 0; }
};

class Stream : public Print {
 public:
  // Pole istnieje, bo FS.h z etapu 196 ustawia _timeout w konstruktorze File.
  unsigned long _timeout = 0;
  unsigned long getTimeout() const { return _timeout; }
  void setTimeout(unsigned long t) { _timeout = t; }
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() {}
};

class IPAddress {
 public:
  IPAddress() {}
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
  String toString() const { return String("0.0.0.0"); }
  operator uint32_t() const { return 0; }
  bool operator==(const IPAddress &) const { return true; }
};

unsigned long millis();
unsigned long micros();
void delay(unsigned long);
uint32_t esp_random();

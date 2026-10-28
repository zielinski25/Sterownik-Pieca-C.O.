// [ETAP 221] Rdzen Arduino/ESP na komputerze - CZESC NASZA, nie biblioteki.
//
// Granica jest swiadoma i opisana w README:
//   * String, Print, Stream, IPAddress, Printable  -> PRAWDZIWE z ArduinoCore-API
//     (narzedzia/prawdziwy-serwer221/biblioteka/arduino-core-api),
//   * ESPAsyncWebServer, WebRequest.cpp, WebServer.cpp, WebResponses.cpp,
//     WebHandlers.cpp, matcher, parser HTTP  -> PRAWDZIWE z biblioteki
//     (narzedzia/prawdziwy-serwer221/biblioteka/espasyncwebserver),
//   * to, co rdzen ESP32 dodaje od siebie (PROGMEM, millis, ESP, Serial)
//     -> tutaj, na komputerze.
#pragma once

#include <ArduinoAPI.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// Rdzen ESP32 definiuje te makra; ArduinoCore-API ich nie ma (to nie jest API,
// to sposob trzymania stalych w pamieci Flash). Na komputerze sa puste.
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef FPSTR
#define FPSTR(p) (reinterpret_cast<const __FlashStringHelper *>(p))
#endif
#ifndef F
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(PSTR(s)))
#endif

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void yield();
void delayMicroseconds(unsigned int us);

// Sprzetowy generator ESP32; na komputerze deterministyczny (ziarno ustawiane
// przez test), zeby wynik etapu byl powtarzalny.
uint32_t esp_random();

long random(long howbig);
void randomSeed(unsigned long seed);

class EspClass {
 public:
  uint32_t getFreeHeap() { return 300000u; }
  uint32_t getMinFreeHeap() { return 200000u; }
  uint32_t getHeapSize() { return 400000u; }
  uint32_t getCpuFreqMHz() { return 240u; }
  void restart();
  void restart(bool force) { (void)force; restart(); }
};
extern EspClass ESP;

// Odpowiednik Serial: wszystko idzie na stdout (logi biblioteki i testu),
// zeby nie mieszac sie z przechwytywana odpowiedzia HTTP.
class HostSerial : public Print {
 public:
  void begin(unsigned long baud = 115200) { (void)baud; }
  void end() {}
  int available() { return 0; }
  int peek() { return -1; }
  int read() { return -1; }
  void flush() {}
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  using Print::write;
};
extern HostSerial Serial;

// [ETAP 221] Rdzen na komputerze - CZESC NASZA.
#include <Arduino.h>

#include <chrono>
#include <cstdlib>
#include <random>

#include "AsyncTCP.h"
#include "WiFi.h"

EspClass ESP;
HostSerial Serial;
WiFiClass WiFi;

static std::mt19937 &generator() {
  static std::mt19937 g(0u);  // ziarno stale: wynik etapu musi byc powtarzalny
  return g;
}

static unsigned long g_start = 0;

// Narzedzie testu (etap 221): przesuniecie zegara bez czekania.
void etap221PrzesunZegar(unsigned long ms) { g_start += ms; }

static unsigned long terazMs() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}

unsigned long millis() { return terazMs() + g_start; }
unsigned long micros() { return millis() * 1000UL; }
void delay(unsigned long ms) { (void)ms; }
void yield() {}
void delayMicroseconds(unsigned int us) { (void)us; }

uint32_t esp_random() { return static_cast<uint32_t>(generator()()); }

long random(long howbig) {
  if (howbig <= 0) return 0;
  return static_cast<long>(generator()() % static_cast<uint32_t>(howbig));
}
void randomSeed(unsigned long seed) { generator().seed(static_cast<uint32_t>(seed)); }

void EspClass::restart() {}

size_t HostSerial::write(uint8_t c) {
  std::fputc(static_cast<int>(c), stdout);
  return 1;
}

size_t HostSerial::write(const uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size == 0) return 0;
  std::fwrite(buffer, 1, size, stdout);
  return size;
}

// Rdzen ESP32 dostarcza te funkcje (cores/esp32/WString.cpp); ArduinoCore-API
// ich nie ma, bo to nie jest API. Nasze trasy ich nie uzywaja - linkuje je
// biblioteka (String::toInt itd.).
extern "C" {

char *ltoa(long value, char *str, int base) {
  if (base == 10) {
    std::snprintf(str, 24, "%ld", value);
    return str;
  }
  unsigned long v = (unsigned long)value;
  char tmp[70];
  int i = 0;
  if (v == 0) tmp[i++] = '0';
  while (v) {
    const int c = (int)(v % (unsigned)base);
    tmp[i++] = (char)(c < 10 ? '0' + c : 'a' + c - 10);
    v /= (unsigned)base;
  }
  int j = 0;
  while (i) str[j++] = tmp[--i];
  str[j] = '\0';
  return str;
}

char *itoa(int value, char *str, int base) {
  if (base == 10) {
    std::snprintf(str, 16, "%d", value);
    return str;
  }
  return ltoa((long)value, str, base);
}
char *utoa(unsigned int value, char *str, int base) { return ltoa((long)value, str, base); }
char *ultoa(unsigned long value, char *str, int base) { return ltoa((long)value, str, base); }

char *dtostrf(double value, signed char width, unsigned char prec, char *str) {
  std::snprintf(str, 64, "%*.*f", (int)width, (int)prec, value);
  return str;
}
}

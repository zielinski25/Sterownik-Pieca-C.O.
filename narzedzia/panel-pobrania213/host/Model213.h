#pragma once
#include <cstdint>

#include "Arduino.h"  // model z etapu 211: String, millis(), delay()

// [etap 213, tylko test] esp_random() z ESP-IDF. W firmware to sprzetowy
// generator; w tescie deterministyczny LCG, zeby bilety byly powtarzalne,
// a jednoczesnie rozne dla kolejnych wywolan.
extern std::uint32_t g_los213;
inline std::uint32_t esp_random() {
  g_los213 = g_los213 * 1664525u + 1013904223u;
  return g_los213;
}

// Ustawienie zegara wirtualnego (millis()) na potrzeby testow waznosci.
inline void zegar213(std::uint32_t t) { g_zegar211 = t; }

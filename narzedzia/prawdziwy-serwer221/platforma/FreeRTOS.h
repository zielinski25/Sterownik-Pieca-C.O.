// [ETAP 221] Minimum FreeRTOS na komputerze - CZESC NASZA.
//
// Nasza bramka w v3.56.4 bierze logMutex przez xSemaphoreTake(logMutex,
// pdMS_TO_TICKS(200)). Na komputerze to std::timed_mutex o tych samych
// zachowaniach: czeka do 200 ms i zwraca pdFALSE, gdy blokada zajeta.
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>

typedef std::timed_mutex *SemaphoreHandle_t;
typedef uint32_t TickType_t;

#define pdTRUE ((int)1)
#define pdFALSE ((int)0)
#define pdMS_TO_TICKS(ms) static_cast<TickType_t>(ms)

inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
  if (s == nullptr) return pdFALSE;
  if (ticks == 0) return s->try_lock() ? pdTRUE : pdFALSE;
  return s->try_lock_for(std::chrono::milliseconds(ticks)) ? pdTRUE : pdFALSE;
}

inline int xSemaphoreGive(SemaphoreHandle_t s) {
  if (s == nullptr) return pdFALSE;
  s->unlock();
  return pdTRUE;
}

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::timed_mutex(); }

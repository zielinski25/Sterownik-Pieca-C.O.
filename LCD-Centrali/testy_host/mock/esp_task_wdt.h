// Atrapa esp_task_wdt.h (ESP-IDF 5.x) - model watchdoga opisany w atrapa.h.
#pragma once
#include "atrapa.h"
#include "esp_err.h"
typedef struct {
  uint32_t timeout_ms;
  uint32_t idle_core_mask;
  bool trigger_panic;
} esp_task_wdt_config_t;
inline esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t* c) { g_atrapa.wdtTimeoutMs = c->timeout_ms; return ESP_OK; }
inline esp_err_t esp_task_wdt_add(void*) { g_atrapa.wdtNadzoruje = true; g_atrapa.wdtOstatnieKarmienieMs = g_atrapa.terazMs; return ESP_OK; }
inline esp_err_t esp_task_wdt_delete(void*) { g_atrapa.wdtNadzoruje = false; return ESP_OK; }
inline esp_err_t esp_task_wdt_reset() { g_atrapa.wdtOstatnieKarmienieMs = g_atrapa.terazMs; return ESP_OK; }

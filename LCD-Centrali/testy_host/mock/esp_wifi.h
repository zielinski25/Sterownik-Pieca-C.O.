// Atrapa esp_wifi.h
#pragma once
#include "atrapa.h"
#include "esp_err.h"
inline esp_err_t esp_wifi_scan_stop() { g_atrapa.liczbaStopSkanu++; g_atrapa.skanTrwa = false; return ESP_OK; }

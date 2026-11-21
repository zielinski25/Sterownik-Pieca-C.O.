// Atrapa esp_sleep.h. Do v1.4.7 panel w galezi LVGL glebokiego snu nie uzywal;
// od v1.5.0 usypia na baterii (idzSpac() w main.cpp). Na plytce
// esp_deep_sleep_start() nie wraca - tu rzuca AtrapaSen, ktory symulator lapie
// w biegnij() (tak jak AtrapaRestart z ESP.restart()); loop() nie jest potem
// wolane, zegar plynie dalej.
#pragma once
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "atrapa.h"
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_ALL, ESP_SLEEP_WAKEUP_EXT0, ESP_SLEEP_WAKEUP_EXT1, ESP_SLEEP_WAKEUP_TIMER } esp_sleep_wakeup_cause_t;
typedef enum { ESP_EXT1_WAKEUP_ALL_LOW = 0, ESP_EXT1_WAKEUP_ANY_HIGH = 1 } esp_sleep_ext1_wakeup_mode_t;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return (esp_sleep_wakeup_cause_t)g_atrapa.wybudzenie; }
inline esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t pin, int poziom) { g_atrapa.snuExt0Pin = (int)pin; g_atrapa.snuExt0Poziom = poziom; return ESP_OK; }
inline esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t maska, esp_sleep_ext1_wakeup_mode_t tryb) { g_atrapa.snuExt1Maska = maska; g_atrapa.snuExt1Tryb = (int)tryb; return ESP_OK; }
[[noreturn]] inline void esp_deep_sleep_start() { g_atrapa.usnal++; g_atrapa.usnalMs = g_atrapa.terazMs; throw AtrapaSen(); }

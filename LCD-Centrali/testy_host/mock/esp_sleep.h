// Atrapa esp_sleep.h - glebokiego snu panel w galezi LVGL nie uzywa.
#pragma once
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_err.h"
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_ALL, ESP_SLEEP_WAKEUP_EXT0, ESP_SLEEP_WAKEUP_EXT1, ESP_SLEEP_WAKEUP_TIMER } esp_sleep_wakeup_cause_t;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_UNDEFINED; }
inline esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t, int) { return ESP_OK; }
inline void esp_deep_sleep_start() { exit(42); }   // w testach nie powinno sie zdarzyc

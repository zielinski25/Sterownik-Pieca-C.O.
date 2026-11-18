// Atrapa driver/gpio.h
#pragma once
#include "esp_err.h"
typedef enum {
  GPIO_NUM_0 = 0, GPIO_NUM_2 = 2, GPIO_NUM_4 = 4, GPIO_NUM_12 = 12, GPIO_NUM_13 = 13, GPIO_NUM_14 = 14, GPIO_NUM_15 = 15,
  GPIO_NUM_21 = 21, GPIO_NUM_25 = 25, GPIO_NUM_26 = 26, GPIO_NUM_27 = 27, GPIO_NUM_32 = 32, GPIO_NUM_33 = 33,
  GPIO_NUM_34 = 34, GPIO_NUM_35 = 35, GPIO_NUM_36 = 36, GPIO_NUM_39 = 39, GPIO_NUM_MAX = 40
} gpio_num_t;
inline esp_err_t gpio_hold_en(gpio_num_t) { return ESP_OK; }
inline esp_err_t gpio_hold_dis(gpio_num_t) { return ESP_OK; }
inline void gpio_deep_sleep_hold_en() {}
inline void gpio_deep_sleep_hold_dis() {}

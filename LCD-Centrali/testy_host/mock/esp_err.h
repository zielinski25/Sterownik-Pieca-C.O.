// Atrapa esp_err.h
#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_WIFI_STATE 0x3006
inline const char* esp_err_to_name(esp_err_t e) {
  switch (e) {
    case ESP_OK: return "ESP_OK";
    case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_INVALID_CRC: return "ESP_ERR_INVALID_CRC";
    case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
    default: return "ESP_ERR(atrapa)";
  }
}

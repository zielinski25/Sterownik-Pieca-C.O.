// Atrapa esp_core_dump.h (ESP-IDF 5.x, espcoredump) - zrzut po awarii w
// partycji coredump. Stan: g_atrapa.zrzut*. Makra CONFIG_* sa tu wlaczone tak,
// jak w bibliotekach Arduino dla ESP32 (sdkconfig: ..._ENABLE_TO_FLASH=y,
// ..._DATA_FORMAT_ELF=y).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "atrapa.h"
#include "esp_err.h"
#define CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH 1
#define CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF 1
#define APP_ELF_SHA256_SZ 17
typedef struct {
  uint32_t bt[16];
  uint32_t depth;
  bool corrupted;
} esp_core_dump_bt_info_t;
typedef struct {
  uint32_t exc_cause;
  uint32_t exc_vaddr;
  uint32_t exc_a[16];
  uint32_t epcx[8];
  uint8_t epcx_reg_bits;
} esp_core_dump_summary_extra_info_t;
typedef struct {
  uint32_t exc_tcb;
  char exc_task[16];
  uint32_t exc_pc;
  esp_core_dump_bt_info_t exc_bt_info;
  uint32_t core_dump_version;
  uint8_t app_elf_sha256[APP_ELF_SHA256_SZ];
  esp_core_dump_summary_extra_info_t ex_info;
} esp_core_dump_summary_t;
inline esp_err_t esp_core_dump_image_check(void) {
  if (!g_atrapa.zrzutJest) return ESP_ERR_NOT_FOUND;
  return g_atrapa.zrzutUszkodzony ? ESP_ERR_INVALID_CRC : ESP_OK;
}
inline esp_err_t esp_core_dump_image_erase(void) { g_atrapa.zrzutJest = false; g_atrapa.zrzutSkasowan++; return ESP_OK; }
inline esp_err_t esp_core_dump_get_panic_reason(char* bufor, size_t n) {
  if (!g_atrapa.zrzutJest) return ESP_ERR_NOT_FOUND;
  snprintf(bufor, n, "%s", g_atrapa.zrzutPowod);
  return ESP_OK;
}
inline esp_err_t esp_core_dump_get_summary(esp_core_dump_summary_t* s) {
  if (!g_atrapa.zrzutJest) return ESP_ERR_NOT_FOUND;
  memset(s, 0, sizeof(*s));
  snprintf(s->exc_task, sizeof(s->exc_task), "%s", g_atrapa.zrzutZadanie);
  s->exc_pc = g_atrapa.zrzutPc;
  s->exc_bt_info.depth = g_atrapa.zrzutSladN;
  for (uint32_t i = 0; i < g_atrapa.zrzutSladN && i < 16; i++) s->exc_bt_info.bt[i] = g_atrapa.zrzutSlad[i];
  return ESP_OK;
}

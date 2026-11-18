// Atrapa esp_ota_ops.h - stan partycji z g_atrapa.otaStanPartycji (domyslnie
// "juz potwierdzona"); potwierdzenia sa liczone (g_atrapa.otaPotwierdzenPartycji).
// Od v1.4.0 takze esp_partition_get_sha256() (na plytce w esp_partition.h):
// suma SHA-256 programu, z ktorego "plytka" pracuje - g_atrapa.otaSumaWlasna;
// g_atrapa.otaSumaBlad = true udaje, ze sumy nie da sie odczytac.
#pragma once
#include <string.h>
#include "atrapa.h"
#include "esp_err.h"
typedef struct { char label[17]; uint32_t size; } esp_partition_t;
typedef enum {
  ESP_OTA_IMG_NEW = 0,
  ESP_OTA_IMG_PENDING_VERIFY = 1,
  ESP_OTA_IMG_VALID = 2,
  ESP_OTA_IMG_INVALID = 3,
  ESP_OTA_IMG_ABORTED = 4,
  ESP_OTA_IMG_UNDEFINED = -1
} esp_ota_img_states_t;
inline const esp_partition_t* esp_ota_get_running_partition() { static esp_partition_t p = {"app0", 0x1F0000}; return &p; }
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) { static esp_partition_t p = {"app1", 0x1F0000}; return &p; }
inline esp_err_t esp_partition_get_sha256(const esp_partition_t*, uint8_t* suma) {
  if (g_atrapa.otaSumaBlad) return ESP_FAIL;
  memcpy(suma, g_atrapa.otaSumaWlasna, 32);
  return ESP_OK;
}
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t*, esp_ota_img_states_t* s) { *s = (esp_ota_img_states_t)g_atrapa.otaStanPartycji; return ESP_OK; }
inline esp_err_t esp_ota_mark_app_valid_cancel_rollback() {
  g_atrapa.otaPotwierdzenPartycji++;
  g_atrapa.otaStanPartycji = ESP_OTA_IMG_VALID;
  return ESP_OK;
}

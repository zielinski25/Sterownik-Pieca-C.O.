// Atrapa esp_heap_caps.h - "wolny RAM plytki" podaje test (g_atrapa.ram*).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "atrapa.h"

#define MALLOC_CAP_8BIT     (1u << 2)
#define MALLOC_CAP_INTERNAL (1u << 11)
#define MALLOC_CAP_DEFAULT  (1u << 12)

inline size_t heap_caps_get_free_size(uint32_t) { return g_atrapa.ramWolny; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return g_atrapa.ramBlok; }
inline size_t heap_caps_get_minimum_free_size(uint32_t) { return g_atrapa.ramMin; }

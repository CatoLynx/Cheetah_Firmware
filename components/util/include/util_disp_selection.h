#pragma once

#include "nvs_flash.h"
#include "cJSON.h"

esp_err_t display_selection_loadConfiguration(nvs_handle_t* nvsHandle, cJSON** json, const char* log_tag);
esp_err_t display_selection_loadAndParseConfiguration(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units, const char* log_tag);
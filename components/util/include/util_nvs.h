#pragma once

#include "esp_system.h"
#include "nvs_flash.h"
#include "cJSON.h"

char* get_string_from_nvs(nvs_handle_t* nvsHandle, const char* key);
esp_err_t get_json_from_spiffs(const char* spiffsFileName, cJSON** json, const char* log_tag);
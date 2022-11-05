#pragma once

#include "esp_system.h"
#include "nvs_flash.h"

char* get_string_from_nvs(nvs_handle_t* nvsHandle, const char* key);